#ifndef BDF_H_INCLUDED__
#define BDF_H_INCLUDED__

#include <string>

namespace BDF {

struct Header
{
	uint16_t SIZE[3];
	int16_t FONTBOUNDINGBOX[4];
	std::string CHARSET_REGISTRY;
	uint16_t CHARS;
};

struct CharacterSegment
{
//	uint16_t STARTCHAR;	// descriptive name
	uint16_t ENCODING;	// decimal code point
//	uint8_t SWIDTH;
	uint8_t DWIDTH;
	int16_t BBX[4]; // BBw BBh BBox BBoy
};

const char* ReadHeader(const char* text, size_t len, Header& header);

void ReadCharacterSegments(
	const char* text, size_t len,
	const Header& header, CharacterSegment* pSegments,
	uint8_t* pData
	);

size_t CalcSegmentMaxDataSize(const Header& header);

} // namespace bdf

#endif // #ifndef BDF_H_INCLUDED__
