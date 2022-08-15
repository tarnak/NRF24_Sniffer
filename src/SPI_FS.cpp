#include "Arduino.h"
#include <SPI.h>
#include <SPIFFS.h>
#include <stdint.h>

#include "main.h"
#include "SPI_FS.h"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED false
#define DEBUG_SPIFFS
#define DEBUG_SPIFFS_INT

#ifdef DEBUG_SPIFFS
#define D_SPIFFS(fmt, ...) DEBUG_D(fmt, __VA_ARGS__)
#else
#define D_SPIFFS(fmt, ...)
#endif

#ifdef DEBUG_SPIFFS_INT
#define D_SPIFFS_INT(fmt, ...) DEBUG_D(fmt, __VA_ARGS__)
#else
#define D_SPIFFS_INT(fmt, ...)
#endif

SPI_FS::SPI_FS()
{
}

SPI_FS::~SPI_FS()
{
}

bool SPI_FS::init(bool listRootDir)
{
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        D_SPIFFS("---- SPIFFS Mount Failed ----\n\n", "");
        return false;
    }

    if (listRootDir)
        listDir(SPIFFS, "/", 0);

    D_SPIFFS("SPIFFS Mount SUCCESS\n\n", "");
    return true;
}

void SPI_FS::listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    D_SPIFFS("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        D_SPIFFS("- failed to open directory\n\n", "");
        return;
    }

    if (!root.isDirectory())
    {
        D_SPIFFS(" - not a directory\n\n", "");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            D_SPIFFS("  DIR : %s\n", file.name());
            if (levels)
            {
                listDir(fs, file.path(), levels - 1);
            }
        }
        else
        {
            D_SPIFFS("  FILE: %s,", file.name());
            D_SPIFFS(" ,SIZE: %d\n", file.size());
        }
        file = root.openNextFile();
    }
    D_SPIFFS("\n", "");
}

void SPI_FS::readFile(fs::FS &fs, const char *path)
{
    D_SPIFFS("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        D_SPIFFS("- failed to open file for reading\n\n", "");
        return;
    }

    D_SPIFFS("- read from file:", "");
    while (file.available())
    {
        D_SPIFFS("%c", file.read());
    }
    file.close();
    D_SPIFFS("\n\n", "");
}

void SPI_FS::writeFile(fs::FS &fs, const char *path, const char *message)
{
    D_SPIFFS("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        D_SPIFFS("- failed to open file for writing\n\n", "");
        return;
    }
    if (file.print(message))
    {
        D_SPIFFS("- file written\n", "");
    }
    else
    {
        D_SPIFFS("- write failed\n", "");
    }
    file.close();
    D_SPIFFS("\n", "");
}

void SPI_FS::appendFile(fs::FS &fs, const char *path, const char *message)
{
    D_SPIFFS("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        D_SPIFFS("- failed to open file for appending\n\n", "");
        return;
    }
    if (file.print(message))
    {
        D_SPIFFS("- message appended\n", "");
    }
    else
    {
        D_SPIFFS("- append failed\n", "");
    }
    file.close();
    D_SPIFFS("\n", "");
}

void SPI_FS::renameFile(fs::FS &fs, const char *path1, const char *path2)
{
    D_SPIFFS("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2))
    {
        D_SPIFFS("- file renamed\n", "");
    }
    else
    {
        D_SPIFFS("- rename failed\n", "");
    }
    D_SPIFFS("\n", "");
}

void SPI_FS::deleteFile(fs::FS &fs, const char *path)
{
    D_SPIFFS("Deleting file: %s\r\n", path);
    if (fs.remove(path))
    {
        D_SPIFFS("- file deleted\n", "");
    }
    else
    {
        D_SPIFFS("- delete failed\n", "");
    }
    D_SPIFFS("\n", "");
}

void SPI_FS::testFileIO(fs::FS &fs, const char *path)
{
    D_SPIFFS("Testing file I/O with %s\r\n", path);

    static uint8_t buf[512];
    size_t len = 0;
    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        D_SPIFFS("- failed to open file for writing\n\n", "");
        return;
    }

    size_t i;
    D_SPIFFS("- writing", "");
    uint32_t start = millis();
    for (i = 0; i < 2048; i++)
    {
        if ((i & 0x001F) == 0x001F)
        {
            D_SPIFFS(".", "");
        }
        file.write(buf, 512);
    }
    Serial.println("");
    uint32_t end = millis() - start;
    D_SPIFFS(" - %u bytes written in %u ms\r\n", 2048 * 512, end);
    file.close();

    file = fs.open(path);
    start = millis();
    end = start;
    i = 0;
    if (file && !file.isDirectory())
    {
        len = file.size();
        size_t flen = len;
        start = millis();
        D_SPIFFS("- reading", "");
        while (len)
        {
            size_t toRead = len;
            if (toRead > 512)
            {
                toRead = 512;
            }
            file.read(buf, toRead);
            if ((i++ & 0x001F) == 0x001F)
            {
                D_SPIFFS(".", "");
            }
            len -= toRead;
        }
        D_SPIFFS("", "");
        end = millis() - start;
        D_SPIFFS("- %u bytes read in %u ms\r\n", flen, end);
        file.close();
    }
    else
    {
        D_SPIFFS("- failed to open file for reading\n", "");
    }
    D_SPIFFS("\n", "");
}

bool SPI_FS::appendInt(fs::FS &fs, const char *path, int iData)
{
    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        D_SPIFFS("appendInt: - failed to open file to append data\n\n", "");
        return false;
    }
    char dBuffer[10];                                 // integer range is -32768 ie 6 digits so allow plenty space
    snprintf(dBuffer, sizeof(dBuffer), " %i", iData); // leading whitespace is ignored by atoi
    if (file.println(dBuffer))
    { // character string ends with a ""
        D_SPIFFS("appendInt to file: %s value: %s\n", path, dBuffer);
    }
    else
    {
        D_SPIFFS("File write failed\n", "");
    }
    file.close();
    D_SPIFFS("\n", "");
    return true;
}

// read a single integer from a SPIFFS file:
int SPI_FS::readInt(fs::FS &fs, const char *path, int defaultValue)
{
    D_SPIFFS("Reading from file: %s\r\n", path);
    File file = fs.open(path);
    if (!file)
    {
        D_SPIFFS("readInt: - failed to open file for reading\n\n", "");
        return (defaultValue);
    }

    char iBuffer[10]; // integer range is -32768 ie 6 chars
    int x = file.readBytesUntil('\n', iBuffer, sizeof(iBuffer) - 1);
    iBuffer[x] = 0; // ensure char array is null terminated for atoi
    int rData = atoi(iBuffer);
    file.close();

    D_SPIFFS_INT("rData is: %s, value: %d\n", iBuffer, rData);
    D_SPIFFS("\n", "");
    return (rData);
}

void SPI_FS::testFileOperations()
{
    listDir(SPIFFS, "/", 0);

    writeFile(SPIFFS, "/hello.txt", "Hello ");
    appendFile(SPIFFS, "/hello.txt", "World!\r\n");
    readFile(SPIFFS, "/hello.txt");

    appendInt(SPIFFS, "/chNumber.txt", 1);
    int val = readInt(SPIFFS, "/chNumber.txt", 1);
    D_SPIFFS("value: %d\n", val);

    listDir(SPIFFS, "/", 0);
}
