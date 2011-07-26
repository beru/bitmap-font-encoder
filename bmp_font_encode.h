#ifndef BMP_FONT_ENCODE_H__
#define BMP_FONT_ENCODE_H__

#include "bmp_font.h"

struct BoxInfo
{
	uint8_t x;
	uint8_t y;
	uint8_t w;
	uint8_t h;
	uint16_t cnt;
	
	uint8_t bitLen;
	uint8_t chc;
};
static inline
bool operator < (const BoxInfo& lhs, const BoxInfo& rhs)
{
	return lhs.cnt < rhs.cnt;
}

std::string EncodeHeader(class BitWriter& bw, const BmpFontHeader& header);

void EncodeBoxTable(class BitWriter& bw, const BoxInfo* pBoxes, uint8_t cnt, uint8_t regIdx, const BmpFontHeader& header);

void Encode(
	class BitWriter& bw, const BitmapFont& bf, const BmpFontHeader& header,
	const BoxInfo* pBoxes, uint8_t cnt, uint8_t regIdx
	);

#endif // #ifndef BMP_FONT_ENCODE_H__
