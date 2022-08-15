#ifndef SPI_FS_H
#define SPI_FS_H

#pragma once

class SPI_FS
{
public:
    SPI_FS();

    ~SPI_FS();

    bool init(bool listRootDir = false);

    void listDir(fs::FS &fs, const char * dirname, uint8_t levels);

    void readFile(fs::FS &fs, const char * path);

    void writeFile(fs::FS &fs, const char * path, const char * message);

    void appendFile(fs::FS &fs, const char * path, const char * message);

    void renameFile(fs::FS &fs, const char * path1, const char * path2);

    void testFileIO(fs::FS &fs, const char * path);

    void deleteFile(fs::FS &fs, const char * path);

    void testFileOperations();

    bool appendInt(fs::FS &fs, const char *path, int iData);

    int readInt(fs::FS &fs, const char *path, int defaultValue);
};



#endif