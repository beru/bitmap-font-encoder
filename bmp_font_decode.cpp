#include "bmp_font_decode.h"

#include "bit_reader.h"
#include "misc.h"

struct FillInfo
{
	uint8_t p1;
	uint8_t p2;
	uint8_t len;
};

uint8_t decodeNum(BitReader& br, uint8_t maxNum)
{
	assert(maxNum >= 1 && maxNum <= 16);
	if (maxNum == 1) {
		return 0;
	}
	uint8_t p2 = pow2roundup(maxNum);
	uint8_t nBits = countBits(p2-1)-1;
	uint8_t ret = 0;
	for (uint8_t i=0; i<nBits; ++i) {
		ret |= br.Pop() << (nBits-1-i);
	}
	if (ret >= p2-maxNum) {
		ret <<= 1;
		ret |= br.Pop();
		ret -= p2-maxNum;
	}
	assert(ret >= 0 && ret <= maxNum);
	return ret;
}

void decodeFills(
	BitReader& br,
	std::vector<FillInfo>& fills,
	uint8_t len1,	// s•ûŒü‚Ì’·‚³
	uint8_t len2	// —ñ•ûŒü‚Ì’·‚³
	)
{
	uint8_t maxLen = decodeNum(br, len2)+1;
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
			offset = decodeNum(br, len2);
			if (len2-offset == 1) {
				fillLen = 1;
			}else {
				assert(len2 >= offset);
				fillLen = decodeNum(br, std::min(maxLen, (uint8_t)(len2-offset))) + 1;
			}
		}else {
			uint8_t diff = len2 - col;
			if (diff == 1) {
				offset = 0;
				fillLen = 1;
			}else {
				offset = decodeNum(br, diff);
				if (len2-(col+offset) == 1) {
					fillLen = 1;
				}else {
					fillLen = decodeNum(br, std::min(maxLen, (uint8_t)(len2-(col+offset)))) + 1;
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
	uint8_t x = decodeNum(br, 16);
	uint8_t y = decodeNum(br, 16);
	
	uint8_t x2 = decodeNum(br, 16-x);
	uint8_t y2 = decodeNum(br, 16-y);
	
	uint8_t w = 16 - x2 - x;
	uint8_t h = 16 - y2 - y;
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

