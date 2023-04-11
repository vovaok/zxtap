#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdint.h>
#include <math.h>
#include <QByteArray>
#include <QVector>

#pragma pack(push,1)
struct WAVEfmt
{
    uint16_t type;
    uint16_t chan;
    int32_t freq;
    int32_t bps;
    uint16_t bytes;
    uint16_t bits;
};

union Sample
{
    int8_t data8[2];
    int16_t data16[2];
    int32_t data32[2];
};

struct TapHeader
{
    uint8_t blockType; // 0x00 - header, 0xff - data
    uint8_t fileType; // 0x00 - Program, 0x01 - Numeric Array, 0x02 - Symbol Array, 0x03 - Bytes
    char name[10];
    uint16_t dataLength;
    uint16_t param; // autorun line or start address
    uint16_t programLength;
//    uint8_t checksum;
};

#pragma pack(pop)

class Block
{
public:
    QByteArray ba;
    int begin;
    int end;
    int bitCount;
    uint8_t checksum;
    QVector<int> byteOffset;

    Block() : begin(0), end(0), bitCount(0), checksum(0) {}
    void clear()
    {
        ba.clear();
        byteOffset.clear();
        begin = end = 0;
        bitCount = 0;
        checksum = 0;
    }
};


//class Butterworth
//{
//private:
//    float x1, x2;
//    float y1, y2;
//    float a1, a2;
//    float b0, b1, b2;
//    float sampleFreq;

//public:
//    Butterworth(float Fsample) : x1(0), x2(0), y1(0), y2(0), sampleFreq(Fsample) {}
//    void setBandPass(float frequency, float bandwidth)
//    {
//        float w1 = tanf(M_PI * (frequency - bandwidth) / sampleFreq);
//        float w2 = tanf(M_PI * (frequency + bandwidth) / sampleFreq);
//        float w0Sqr = w2 * w1;
//        float wd = w2 - w1;
//        float a0 = -1 - wd - w0Sqr;
//        a1 = (2 - 2 * w0Sqr) / a0;
//        a2 = (-1 + wd - w0Sqr) / a0;
//        b0 = (-wd) / a0;
//        b1 = 0 / a0;
//        b2 = (wd) / a0;
//    }
//    float filter(float x)
//    {
//        float x0 = x;
//        float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
//        x2 = x1;
//        x1 = x0;
//        y2 = y1;
//        y1 = y0;
//        return y0;
//    }
//};


#endif // STRUCTS_H
