#include <Arduino.h>
#include <SPI.h>
#include <CircularBuffer/CircularBuffer.h>
#include <RF24.h>
#include <RF24_config.h>

// #define LED_SUPPORTED

// Hardware configuration

#if defined(__XTENSA__)
#define RF_CE_PIN (12)
#define RF_CS_PIN (5)
#define RF_IRQ_PIN (13)
#define RF_IRQ (RF_IRQ_PIN) //

#else
#define RF_CE_PIN (9)
#define RF_CS_PIN (10)
#define RF_IRQ_PIN (2)
#define RF_IRQ (RF_IRQ_PIN - 2) // Usually the interrupt = pin -2 (on uno/nano anyway)
#endif

#ifdef LED_SUPPORTED
#define LED_PIN_LISTEN (A0)
#define LED_PIN_RX (A1)
#define LED_PIN_TX (A2)
#define LED_PIN_CONFIG (A3)
#define LED_PIN_BUFF_FULL (A4)
#endif

#define RF_MAX_ADDR_WIDTH (5) // Maximum address width, in bytes. MySensors use 5 bytes for addressing, where lowest byte is for node addressing.
#define MAX_RF_PAYLOAD_SIZE (32)
#define SER_BAUDRATE (115200)
#define PACKET_BUFFER_SIZE (30) // Maximum number of packets that can be buffered between reception by NRF and transmission over serial port.
#define PIPE (0)                // Pipe number to use for listening

// Startup defaults until user reconfigures it
#define DEFAULT_RF_CHANNEL (76)                   // 76 = Default channel for MySensors.
#define DEFAULT_RF_DATARATE (RF24_1MBPS)          // Datarate
#define DEFAULT_RF_ADDR_WIDTH (RF_MAX_ADDR_WIDTH) // We use all but the lowest address byte for promiscuous listening. First byte of data received will then be the node address.
#define DEFAULT_RF_ADDR_PROMISC_WIDTH (DEFAULT_RF_ADDR_WIDTH - 1)
//#define DEFAULT_RADIO_ID               ((uint64_t)0xABCDABC000LL)                             // 0xABCDABC000LL = MySensors v1 (1.3) default
#define DEFAULT_RADIO_ID ((uint64_t)0xA8A8E1FC00LL)   // 0xA8A8E1FC00LL = MySensors v2 (1.4) default
#define DEFAULT_RF_CRC_LENGTH (2)                     // Length (in bytes) of NRF24 CRC
#define DEFAULT_RF_PAYLOAD_SIZE (MAX_RF_PAYLOAD_SIZE) // Define NRF24 payload size to maximum, so we'll slurp as many bytes as possible from the packet.

// If BINARY_OUTPUT is defined, this sketch will output in hex format to the PC.
// If undefined it will output text output for development.
#define BINARY_OUTPUT

#include "NRF24_sniff_types.h"

#ifndef BINARY_OUTPUT
int my_putc(char c, FILE *t)
{
  Serial.write(c);
}
#endif

// Set up nRF24L01 radio on SPI bus plus CE/CS pins
static RF24 radio(RF_CE_PIN, RF_CS_PIN); // CE, CS = SS

static NRF24_packet_t bufferData[PACKET_BUFFER_SIZE];
static CircularBuffer<NRF24_packet_t> packetBuffer(bufferData, sizeof(bufferData) / sizeof(bufferData[0]));
static Serial_header_t serialHdr;
static volatile Serial_config_t conf = {
    DEFAULT_RF_CHANNEL, DEFAULT_RF_DATARATE, DEFAULT_RF_ADDR_WIDTH,
    DEFAULT_RF_ADDR_PROMISC_WIDTH, DEFAULT_RADIO_ID, DEFAULT_RF_CRC_LENGTH,
    DEFAULT_RF_PAYLOAD_SIZE};

#define GET_PAYLOAD_LEN(p) ((p->packet[conf.addressLen - conf.addressPromiscLen] & 0xFC) >> 2) // First 6 bits of nRF header contain length.

inline static void dumpData(uint8_t *p, int len)
{
#ifndef BINARY_OUTPUT
  while (len--)
  {
    printf("%02x", *p++);
  }
  Serial.print(' ');
#else
  Serial.write(p, len);
#endif
}

static void handleNrfIrq()
{
  static uint8_t lostPacketCount = 0;
  // Loop until RX buffer(s) contain no more packets.
  while (radio.available())
  {
#ifdef LED_SUPPORTED
    digitalWrite(LED_PIN_RX, HIGH);
#endif
    if (!packetBuffer.full())
    {
#ifdef LED_SUPPORTED
      digitalWrite(LED_PIN_BUFF_FULL, LOW);
#endif
      NRF24_packet_t *p = packetBuffer.getFront();
      p->timestamp = micros(); // Micros does not increase in interrupt, but it can be used.
      p->packetsLost = lostPacketCount;
      uint8_t packetLen = radio.getPayloadSize();
      if (packetLen > MAX_RF_PAYLOAD_SIZE)
        packetLen = MAX_RF_PAYLOAD_SIZE;

      radio.read(p->packet, packetLen);

      // Determine length of actual payload (in bytes) received from NRF24 packet control field (bits 7..2 of byte with offset 1)
      // Enhanced shockburst format is assumed!
      if (GET_PAYLOAD_LEN(p) <= MAX_RF_PAYLOAD_SIZE)
      {
        // Seems like a valid packet. Enqueue it.
        packetBuffer.pushFront(p);
      }
      else
      {
        // Packet with invalid size received. Could increase some counter...
      }
      lostPacketCount = 0;
    }
    else
    {
      // Buffer full. Increase lost packet counter.
#ifdef LED_SUPPORTED
      digitalWrite(LED_PIN_BUFF_FULL, HIGH);
#endif
      bool tx_ok, tx_fail, rx_ready;
      if (lostPacketCount < 255)
        lostPacketCount++;
      // Call 'whatHappened' to reset interrupt status.
      radio.whatHappened(tx_ok, tx_fail, rx_ready);
      // Flush buffer to drop the packet.
      radio.flush_rx();
    }
#ifdef LED_SUPPORTED
    digitalWrite(LED_PIN_RX, LOW);
#endif
  }
}

static void activateConf(void)
{
#ifdef LED_SUPPORTED
  digitalWrite(LED_PIN_CONFIG, HIGH);
#endif

  // Match MySensors' channel & datarate
  radio.setChannel(conf.channel);
  radio.setDataRate((rf24_datarate_e)conf.rate);

  // Disable CRC & set fixed payload size to allow all packets captured to be returned by Nrf24.
  radio.disableCRC();
  radio.setPayloadSize(conf.maxPayloadSize);

  // Configure listening pipe with the 'promiscuous' address and start listening
  radio.setAddressWidth(conf.addressPromiscLen);
  radio.openReadingPipe(PIPE, conf.address >> (8 * (conf.addressLen - conf.addressPromiscLen)));
  radio.startListening();

  // Attach interrupt handler to NRF IRQ output. Overwrites any earlier handler.
  attachInterrupt(RF_IRQ, handleNrfIrq, FALLING); // NRF24 Irq pin is active low.

  // Initialize serial header's address member to promiscuous address.
  uint64_t addr = conf.address; // TODO: probably add some shifting!
  for (int8_t i = sizeof(serialHdr.address) - 1; i >= 0; --i)
  {
    serialHdr.address[i] = addr;
    addr >>= 8;
  }

  // Send config back. Write record length & message type
  uint8_t lenAndType = SET_MSG_TYPE(sizeof(conf), MSG_TYPE_CONFIG);
  dumpData(&lenAndType, sizeof(lenAndType));
  // Write config
  dumpData((uint8_t *)&conf, sizeof(conf));

#ifndef BINARY_OUTPUT
  Serial.print("Channel:     ");
  Serial.println(conf.channel);
  Serial.print("Datarate:    ");
  switch (conf.rate)
  {
  case 0:
    Serial.println("1Mb/s");
    break;
  case 1:
    Serial.println("2Mb/s");
    break;
  case 2:
    Serial.println("250Kb/s");
    break;
  }
  Serial.print("Address:     0x");
  uint64_t adr = conf.address;
  for (int8_t i = conf.addressLen - 1; i >= 0; --i)
  {
    if (i >= conf.addressLen - conf.addressPromiscLen)
    {
      Serial.print((uint8_t)(adr >> (8 * i)), HEX);
    }
    else
    {
      Serial.print("**");
    }
  }
  Serial.println("");
  Serial.print("Max payload: ");
  Serial.println(conf.maxPayloadSize);
  Serial.print("CRC length:  ");
  Serial.println(conf.crcLength);
  Serial.println("");

  radio.printDetails();

  Serial.println("");
  Serial.println("Listening...");
#endif
#ifdef LED_SUPPORTED
  digitalWrite(LED_PIN_CONFIG, LOW);
#endif
}

void setup(void)
{
#ifdef LED_SUPPORTED
  pinMode(LED_PIN_LISTEN, OUTPUT);
  pinMode(LED_PIN_RX, OUTPUT);
  pinMode(LED_PIN_TX, OUTPUT);
  pinMode(LED_PIN_CONFIG, OUTPUT);
  pinMode(LED_PIN_BUFF_FULL, OUTPUT);
  digitalWrite(LED_PIN_LISTEN, LOW);
  digitalWrite(LED_PIN_RX, LOW);
  digitalWrite(LED_PIN_TX, LOW);
  digitalWrite(LED_PIN_CONFIG, LOW);
  digitalWrite(LED_PIN_BUFF_FULL, LOW);
#endif

  Serial.begin(SER_BAUDRATE);

#ifndef BINARY_OUTPUT
  fdevopen(&my_putc, 0);
  Serial.println("-- RF24 Sniff --");
#endif

  radio.begin();

  // Disable shockburst
  radio.setAutoAck(false);
  radio.setRetries(0, 0);

  // Configure nRF IRQ input
  pinMode(RF_IRQ_PIN, INPUT);

#ifdef LED_SUPPORTED
  digitalWrite(LED_PIN_LISTEN, HIGH);
#endif

  activateConf();
}

void loop(void)
{
  while (!packetBuffer.empty())
  {
#ifdef LED_SUPPORTED
    digitalWrite(LED_PIN_TX, HIGH);
#endif
    // One or more records present
    NRF24_packet_t *p = packetBuffer.getBack();
    int serialHdrLen = sizeof(serialHdr) - (conf.addressLen - conf.addressPromiscLen);
    serialHdr.timestamp = p->timestamp;
    serialHdr.packetsLost = p->packetsLost;

    // Calculate data length in bits, then round up to get full number of bytes.
    uint8_t dataLen = ((serialHdrLen << 3)                                 /* Serial packet header */
                       + ((conf.addressLen - conf.addressPromiscLen) << 3) /* NRF24 LSB address byte(s) */
                       + 9                                                 /* NRF24 control field */
                       + (GET_PAYLOAD_LEN(p) << 3)                         /* NRF24 payload length */
                       + (conf.crcLength << 3)                             /* NRF24 crc length */
                       + 7                                                 /* Round up to full nr. of bytes */
                       ) >>
                      3; /* Convert from bits to bytes */

    // Write record length & message type
    uint8_t lenAndType = SET_MSG_TYPE(dataLen, MSG_TYPE_PACKET);
    dumpData(&dataLen, sizeof(lenAndType));
    // Write serial header
    dumpData((uint8_t *)&serialHdr, serialHdrLen);
    // Write packet data
    dumpData(p->packet, dataLen - serialHdrLen);

#ifndef BINARY_OUTPUT
    if (p->packetsLost > 0)
    {
      Serial.print(" Lost: ");
      Serial.print(p->packetsLost);
    }
    Serial.println("");
#endif
    // Remove record as we're done with it.
    packetBuffer.popBack();
#ifdef LED_SUPPORTED
    digitalWrite(LED_PIN_TX, LOW);
#endif
  }

  // Test if new config comes in
  uint8_t lenAndType;
  if (Serial.available() >= sizeof(lenAndType) + sizeof(conf))
  {
    lenAndType = Serial.read();
    if ((GET_MSG_TYPE(lenAndType) == MSG_TYPE_CONFIG) && (GET_MSG_LEN(lenAndType) == sizeof(conf)))
    {
      // Disable nRF interrupt while reading & activating new configuration.
      noInterrupts();
      // Retrieve the new configuration
      uint8_t *c = (uint8_t *)(&conf);
      for (uint8_t i = 0; i < sizeof(conf); ++i)
      {
        *c++ = Serial.read();
      }
      // Clear any packets in the buffer and flush rx buffer.
      packetBuffer.clear();
      radio.flush_rx();
      // Activate new config & re-enable nRF interrupt.
      activateConf();

      interrupts();
    }
    else
    {
#ifndef BINARY_OUTPUT
      Serial.println("Illegal configuration received!");
#endif
    }
  }
}
// vim:cin:ai:sts=2 sw=2 ft=cpp1
