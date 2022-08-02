/*
The MIT License (MIT)

Copyright (c) 2013-2015 SRS(ossrs)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "bit_stream.h"

using namespace std;

SrsStream::SrsStream()
{
    p = bytes = NULL;
    nb_bytes = 0;
}

SrsStream::~SrsStream()
{
}

int SrsStream::initialize(char* b, int nb)
{
    int ret = 0;
    
    if (!b) {
        ret = -1;
        return ret;
    }
    
    if (nb <= 0) {
        ret = -1;
        return ret;
    }

    nb_bytes = nb;
    p = bytes = b;

    return ret;
}

char* SrsStream::data()
{
    return bytes;
}

int SrsStream::size()
{
    return nb_bytes;
}

int SrsStream::pos()
{
    return (int)(p - bytes);
}

bool SrsStream::empty()
{
    return !bytes || (p >= bytes + nb_bytes);
}

bool SrsStream::require(int required_size)
{    
    return required_size <= nb_bytes - (p - bytes);
}

void SrsStream::skip(int size)
{    
    p += size;
}

int8_t SrsStream::read_1bytes()
{
    return (int8_t)*p++;
}

int16_t SrsStream::read_2bytes()
{
    int16_t value;
    char* pp = (char*)&value;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

int32_t SrsStream::read_3bytes()
{
    int32_t value = 0x00;
    char* pp = (char*)&value;
    pp[2] = *p++;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

int32_t SrsStream::read_4bytes()
{
    int32_t value;
    char* pp = (char*)&value;
    pp[3] = *p++;
    pp[2] = *p++;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

int64_t SrsStream::read_8bytes()
{
    int64_t value;
    char* pp = (char*)&value;
    pp[7] = *p++;
    pp[6] = *p++;
    pp[5] = *p++;
    pp[4] = *p++;
    pp[3] = *p++;
    pp[2] = *p++;
    pp[1] = *p++;
    pp[0] = *p++;
    
    return value;
}

string SrsStream::read_string(int len)
{
    std::string value;
    value.append(p, len);
    
    p += len;
    
    return value;
}

void SrsStream::read_bytes(char* data, int size)
{
    memcpy(data, p, size);
    
    p += size;
}

void SrsStream::write_1bytes(int8_t value)
{
    *p++ = value;
}

void SrsStream::write_2bytes(int16_t value)
{
    char* pp = (char*)&value;
    *p++ = pp[1];
    *p++ = pp[0];
}

void SrsStream::write_4bytes(int32_t value)
{
    char* pp = (char*)&value;
    *p++ = pp[3];
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void SrsStream::write_3bytes(int32_t value)
{
    char* pp = (char*)&value;
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void SrsStream::write_8bytes(int64_t value)
{
    char* pp = (char*)&value;
    *p++ = pp[7];
    *p++ = pp[6];
    *p++ = pp[5];
    *p++ = pp[4];
    *p++ = pp[3];
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void SrsStream::write_string(string value)
{
    memcpy(p, value.data(), value.length());
    p += value.length();
}

void SrsStream::write_bytes(char* data, int size)
{
    memcpy(p, data, size);
    p += size;
}

SrsBitStream::SrsBitStream()
{
    cb = 0;
    cb_left = 0;
    stream = NULL;
}

SrsBitStream::~SrsBitStream()
{
}

int SrsBitStream::initialize(SrsStream* s) {
    stream = s;
    return 0;
}

bool SrsBitStream::empty() {
    if (cb_left) {
        return false;
    }
    return stream->empty();
}

int8_t SrsBitStream::read_bit() {
    if (!cb_left) {
        cb = stream->read_1bytes();
        cb_left = 8;
    }
    
    int8_t v = (cb >> (cb_left - 1)) & 0x01;
    cb_left--;
    return v;
}

