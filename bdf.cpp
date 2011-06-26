#include "bdf.h"

#include "misc.h"

namespace BDF {

const char* nextLine(const char* text, size_t len)
{
	for (size_t i=0; i<len; ++i) {
		char c = text[i];
		switch (c) {
		case 0:
			return 0;
		case '\r':
			if (i == len-1) {
				return 0;
			}
			if (text[i+1] == '\n') {
				continue;
			}else {
				return text + i + 1;
			}
			break;
		case '\n':
			if (i == len-1) {
				return 0;
			}
			return text + i + 1;
		}
	}
	return 0;
}

const char* skipSpaces(const char* str)
{
	while (*str == ' ') {
		++str;
	}
	return str;
}

template <typename T>
size_t readInteger(const char* line, T& value)
{
	const char* str = skipSpaces(line);
	size_t i = 0;
	do {
		char c = str[i];
		if (c >= '0' && c <= '9') {
			++i;
		}else {
			break;
		}
	}while (1);
	value = atoi(str);
	return (str+i) - line;
}

template <typename T>
void readIntegers(const char* line, T* values, size_t cnt)
{
	for (size_t i=0; i<cnt; ++i) {
		line += readInteger(line, values[i]);
	}
}

void readString(const char* line, std::string& str)
{
	line = skipSpaces(line);
}

void readHeaderLine(const char* line, Header& header)
{
	if (strncmp("SIZE", line, 4) == 0) {
		readIntegers(line+4, header.SIZE, 3);
	}else if (strncmp("FONTBOUNDINGBOX", line, 15) == 0) {
		readIntegers(line+15, header.FONTBOUNDINGBOX, 4);
	}else if (strncmp("CHARSET_REGISTRY", line, 16) == 0) {
		readString(line+16, header.CHARSET_REGISTRY);
	}else if (strncmp("CHARS", line, 5) == 0) {
		readInteger(line+5, header.CHARS);
	}
}

const char* ReadHeader(const char* text, size_t len, Header& header)
{
	const char* line = text;
	size_t remainLen = len;
	do {
		if (remainLen < 20) {
			return 0;
		}
		if (strncmp("STARTCHAR", line, 9) == 0) {
			return line;
		}
		
		readHeaderLine(line, header);
		
		line = nextLine(line, remainLen);
		remainLen = len - (line - text);
	}while (line);
	return 0;
}

const char* readCharacterSegment(
	const char* str, size_t len,
	const Header& header, CharacterSegment& segment,
	uint8_t* pData
	)
{
	size_t remainLen = len;
	size_t nLine = 0;
	const char* line = str;
	do {
		switch (nLine) {
		case 0:
			if (strncmp("STARTCHAR", line, 9) != 0) {
				return 0;
			}
			break;
		case 1:
			if (strncmp("ENCODING", line, 8) != 0) {
				return 0;
			}
			readInteger(line+8, segment.ENCODING);
			break;
		case 2:
			if (strncmp("SWIDTH", line, 6) != 0) {
				return 0;
			}
//			readIntegers(line+6, segment.DWIDTH
			break;
		case 3:
			if (strncmp("DWIDTH", line, 6) != 0) {
				return 0;
			}
			readInteger(line+6, segment.DWIDTH);
			break;
		case 4:
			if (strncmp("BBX", line, 3) != 0) {
				return 0;
			}
			readIntegers(line+3, segment.BBX, 4);
			break;
		case 5:
			if (strncmp("BITMAP", line, 6) != 0) {
				return 0;
			}
			break;
		default:
			if (nLine == 6 + segment.BBX[1]) {
				if (strncmp("ENDCHAR", line, 7) == 0) {
					return nextLine(line, remainLen);
				}
			}else {
				size_t nBytes = (segment.BBX[0] + 7) / 8;
				size_t offset = (nLine - 6) * 2;
				readHexBytes(line, &pData[offset], nBytes);
			}
			break;
		}
		line = nextLine(line, remainLen);
		remainLen = len - (line - str);
		++nLine;
	}while (1);
	return 0;
}

void ReadCharacterSegments(
	const char* text, size_t len,
	const Header& header, CharacterSegment* pSegments,
	uint8_t* pData
	)
{
	size_t segSize = CalcSegmentMaxDataSize(header);
	const char* str = text;
	for (size_t i=0; i<header.CHARS; ++i) {
		CharacterSegment& seg = pSegments[i];
		str = readCharacterSegment(str, len - (str - text), header, seg, &pData[i*segSize]);
		if (!str) {
			if (i != header.CHARS-1) {
				printf("failed to read character segment.\r\n");
			}
			break;
		}
	}
}

size_t CalcSegmentMaxDataSize(const Header& header)
{
	return (header.FONTBOUNDINGBOX[0] + 7) / 8 * header.FONTBOUNDINGBOX[1];
}

} // namespace bdf
