#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdint.h>
#include <math.h>
#include <QByteArray>
#include <QVector>
#include <QColor>
#include <QImage>

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

struct ZxScreenAttr
{
    uint8_t ink: 3;
    uint8_t paper: 3;
    uint8_t bright: 1;
    uint8_t flash: 1;
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
    QImage thumb;
    bool valid;

    Block() : begin(0), end(0), bitCount(0), checksum(0), valid(false) {}
    void clear()
    {
        ba.clear();
        byteOffset.clear();
        begin = end = 0;
        bitCount = 0;
        checksum = 0;
        valid = false;
    }
};

class ZxScreen
{
public:
    ZxScreen(char *data)
    {
        m_data = reinterpret_cast<uint8_t*>(data);
        m_attributes = reinterpret_cast<ZxScreenAttr*>(m_data + 6144);
    }

    uint32_t getPixel(int x, int y)
    {
        int bt = 32*(8*(y%8)+(y%64)/8+y/64*64)+x/8;
        int bit = 7-x%8;
        int abt = x/8+32*(y/8);
        uint8_t byte = m_data[bt];
        ZxScreenAttr attr = m_attributes[abt];
//        int pixel = (byte % int(pow(2, (bit+1)))) / int(pow(2, bit));
        bool pixel = byte & (1 << bit);

        uint32_t col;
        if (pixel)
            col = zxColor(attr.ink, attr.bright);
        else
            col = zxColor(attr.paper, attr.bright);
        col |= 0xff000000;
//        col += 0xff000000 * attr.flash;

        return col;
    }

    QImage toImage()
    {
        QImage img(256, 192, QImage::Format_ARGB32_Premultiplied);
        for (int y=0; y<192; y++)
        {
            for (int x=0; x<256; x++)
            {
                img.setPixel(x, y, getPixel(x, y));
            }
        }
        return img;
    }

//    void setPixel(int x, int y, uint8_t attr);
//    void resetPixel(int x, int y, uint8_t attr);

private:
    //    uint8_t data[6912];
    uint8_t *m_data = nullptr;
    ZxScreenAttr *m_attributes = nullptr;

    uint32_t zxColor(int col, int br)
    {
        switch (col)
        {
        case 0: return br? 0x00000000: 0x00000000;
        case 1: return br? 0x000000ff: 0x000000c0;
        case 2: return br? 0x00ff0000: 0x00c00000;
        case 3: return br? 0x00ff00ff: 0x00c000c0;
        case 4: return br? 0x0000ff00: 0x0000c000;
        case 5: return br? 0x0000ffff: 0x0000c0c0;
        case 6: return br? 0x00ffff00: 0x00c0c000;
        case 7: return br? 0x00ffffff: 0x00c0c0c0;
        }
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
