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

// 1 origin
inline
uint8_t calcNumBits(uint8_t len)
{
	return (uint8_t) std::ceil(std::log((double)len) / std::log(2.0));
}

inline
uint8_t pow2roundup(uint8_t x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    return x+1;
}

inline
uint8_t countBits(uint8_t n)
{
	n = (uint8_t)( ((n & 0xAA) >> 1) + (n & 0x55) );
	n = (uint8_t)( ((n & 0xCC) >> 2) + (n & 0x33) );
	n = (uint8_t)( ((n & 0xF0) >> 4) + (n & 0x0F) );
	return n;
}

