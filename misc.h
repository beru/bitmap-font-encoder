#pragma once

#include <cmath>

inline
uint8_t hexCharToInt(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}else {
		assert(false);
	}
	return 0;
}

inline
bool isHex(char c)
{
	return (0
		|| (c >= '0' && c <= '9')
		|| (c >= 'A' && c <= 'F')
		|| (c >= 'a' && c <= 'f')
		);
}

inline
uint8_t readHexByte(char c0, char c1)
{
	if (isHex(c0) && isHex(c1)) {
		return hexCharToInt(c0) * 16 + hexCharToInt(c1);
	}else {
		return 0;
	}
}

inline
void readHexBytes(const char* str, uint8_t* data, size_t nBytes)
{
	for (size_t i=0; i<nBytes; ++i) {
		data[i] = readHexByte(str[0], str[1]);
		str += 2;
	}
}

// Round up to next higher power of 2 (return x if it's already a power of 2).
inline
uint8_t pow2roundup(uint8_t x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    return x+1;
}

// Round up to next higher power of 2 (return x if it's already a power of 2).
inline
uint16_t pow2roundup16(uint16_t x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    return x+1;
}

// Round up to next higher power of 2 (return x if it's already a power of 2).
inline
uint32_t pow2roundup32(uint32_t x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

inline
uint8_t countBits32(uint32_t n)
{
	n = ((n & 0xAAAAAAAA) >>  1) + (n & 0x55555555);
	n = ((n & 0xCCCCCCCC) >>  2) + (n & 0x33333333);
	n = ((n & 0xF0F0F0F0) >>  4) + (n & 0x0F0F0F0F);
	n = ((n & 0xFF00FF00) >>  8) + (n & 0x00FF00FF);
	n = ((n & 0xFFFF0000) >> 16) + (n & 0x0000FFFF);
	return n;
}

inline
uint8_t countBits16(uint16_t n)
{
	n = (unsigned short)( ((n & 0xAAAA) >> 1) + (n & 0x5555) );
	n = (unsigned short)( ((n & 0xCCCC) >> 2) + (n & 0x3333) );
	n = (unsigned short)( ((n & 0xF0F0) >> 4) + (n & 0x0F0F) );
	n = (unsigned short)( ((n & 0xFF00) >> 8) + (n & 0x00FF) );
	return (uint8_t) n;
}

inline
uint8_t countBits8(uint8_t n)
{
	n = (uint8_t)( ((n & 0xAA) >> 1) + (n & 0x55) );
	n = (uint8_t)( ((n & 0xCC) >> 2) + (n & 0x33) );
	n = (uint8_t)( ((n & 0xF0) >> 4) + (n & 0x0F) );
	return n;
}

/*
inline
uint8_t ntz(uint8_t n)
{
	int x = n;
	return countBits8((x&(-x))-1);
}
*/

inline
uint8_t ntz(uint16_t n)
{
	int x = n;
	return countBits16((x&(-x))-1);
}

