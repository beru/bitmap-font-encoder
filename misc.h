#pragma once

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

