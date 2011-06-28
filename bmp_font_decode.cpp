#include "bmp_font_decode.h"

#include "bit_reader.h"
#include "misc.h"

struct FillInfo
{
	uint8_t p1;
	uint8_t p2;
	uint8_t len;
};

uint8_t decodeNum(BitReader& br, uint8_t nBits)
{
	assert(nBits >= 1 && nBits <= 8);
	uint8_t ret = 0;
	for (uint8_t i=0; i<nBits; ++i) {
		if (br.Pop()) {
			ret += (1 << i);
		}
	}
	return ret;
}

void decodeFills(
	BitReader& br,
	std::vector<FillInfo>& fills,
	uint8_t len1,	// s•ûŒü‚Ì’·‚³
	uint8_t len2	// —ñ•ûŒü‚Ì’·‚³
	)
{
	const uint8_t posBitLen = calcNumBits(len2);
	uint8_t lenBitLen = decodeNum(br, 2) + 1;
	uint8_t row = 0;
	uint8_t col = 0;
	while (row < len1) {
		bool b = br.Pop();
		if (!b) {
			++row;
			col = 0;
			continue;
		}
		uint8_t offset;
		uint8_t fillLen;
		if (col == 0) {
			offset = decodeNum(br, posBitLen);
			uint8_t maxLenBitLen = calcNumBits(len2-(col+offset));
			if (maxLenBitLen == 0) {
				fillLen = 1;
			}else {
				fillLen = decodeNum(br, std::min(lenBitLen, maxLenBitLen)) + 1;
			}
		}else {
			uint8_t diff = len2 - col;
			if (diff == 1) {
				offset = 0;
				fillLen = 1;
			}else {
				uint8_t nBits = std::min(posBitLen, calcNumBits(diff));
				offset = decodeNum(br, nBits);
				uint8_t maxLenBitLen = calcNumBits(len2-(col+offset));
				if (maxLenBitLen == 0) {
					fillLen = 1;
				}else {
					fillLen = decodeNum(br, std::min(lenBitLen, maxLenBitLen)) + 1;
				}
			}
		}
		FillInfo fi;
		fi.p1 = row;
		fi.p2 = col + offset;
		fi.len = fillLen;
		fills.push_back(fi);
		col += offset + fillLen + 1;
		if (col >= len2) {
			++row;
			col = 0;
		}
	}
}

void Decode(BitmapFont& bf, BitReader& br)
{
	uint8_t x = decodeNum(br, 4);
	uint8_t y = decodeNum(br, 4);
	uint8_t w = decodeNum(br, 4) + 1;
	uint8_t h = decodeNum(br, 4) + 1;
	bf.Init(w, h);
	bf.x_ = x;
	bf.y_ = y;
	
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	decodeFills(br, hFills, h, w);
	decodeFills(br, vFills, w, h);
	
	for (size_t i=0; i<hFills.size(); ++i) {
		const FillInfo& fi = hFills[i];
		for (uint8_t j=0; j<fi.len; ++j) {
			bf.FillPixel(fi.p2+j, fi.p1);
		}
	}
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		for (uint8_t j=0; j<fi.len; ++j) {
			bf.FillPixel(fi.p1, fi.p2+j);
		}
	}
}

