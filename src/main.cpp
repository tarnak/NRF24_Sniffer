/*
 * See documentation at https://nRF24.github.io/RF24
 * See License information at root directory of this library
 * Author: Brendan Doherty (2bndy5)
 */

/**
 * A simple example of sending data from 1 nRF24L01 transceiver to another.
 *
 * This example was written to be used on 2 devices acting as "nodes".
 * Use the Serial Monitor to change each node's behavior.
 */
#include "Arduino.h"
#include <SPI.h>

#include "nRF24L01.h"
#include "RF24.h"

// #define TRANSMITTER // otherwise receiver

const byte DEBUG = true;

float msg[3] = {33.33, 66.66, 99.99};

//  RF24(uint16_t _cepin, uint16_t _cspin, uint16_t sck=-1, uint16_t miso=-1, uint16_t mosi=-1);
RF24 radio(12, 5, 18, 19, 23);
// RF24 radio(12, 5); // using pin 12 for the CE pin, and pin 5 for the CSN pin

const uint64_t pip = 0xE8E8F0F0E1LL;

void setupTransmitter(void)
{

    radio.begin();
    radio.setChannel(2);
    radio.setPayloadSize(16);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_HIGH);
    radio.openWritingPipe(pip);
    Serial.println("transmitter...");
}

void loopTransmitter(void)
{
    Serial.print("send... ");
    bool isSuccess = radio.write(&msg, sizeof(msg));
    Serial.println(isSuccess ? "success" : "failed");
    delay(3000);
}

// Odbiornik ESP32
float buff[3];

// RF24 radio(12, 14, 26, 25, 27);
// RF24 radio(7,8);

uint32_t t = 0;
uint32_t t_start = 0;

void setupReceiver(void)
{
    radio.begin();
    radio.setChannel(2);
    radio.setPayloadSize(16);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_HIGH);
    radio.openReadingPipe(1, pip);
    radio.startListening();

    Serial.println("receiver...");
    t_start = millis();
}

void loopReceiver(void)
{
    if (radio.available())
    {
        radio.read(buff, sizeof(buff));

        delay(10);
        Serial.println(buff[0]);
        Serial.println(buff[1]);
        Serial.println(buff[2]);
        if (DEBUG)
            Serial.println(t);

        t_start = millis();
    }
    else
    {
        t = millis() - t_start;
        if (DEBUG)
        {
            Serial.println(t);
            delay(1000);
        }
    }

    if (t > 5000)
    {
        Serial.println("ERROR!");
        delay(1000);
    }
}

void setup(void)
{
    Serial.begin(115200);

#ifdef TRANSMITTER
    setupTransmitter();
#else
    setupReceiver();
#endif
}

void loop(void)
{
#ifdef TRANSMITTER
    loopTransmitter();
#else
    loopReceiver();
#endif
}
