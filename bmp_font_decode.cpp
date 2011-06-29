#include "bmp_font_decode.h"

#include "bit_reader.h"
#include "misc.h"

struct FillInfo
{
	uint8_t p1;
	uint8_t p2;
	uint8_t len;
};

#if 1

// need to reduce branches....
uint8_t decodeNum(BitReader& br, uint8_t maxNum)
{
	assert(maxNum >= 1 && maxNum <= 16);
	uint8_t ret = 1;
	switch (maxNum) {
	case 1:
		break;
	case 2:
		if (br.Pop()) {
			ret += 1;
		}
		break;
	case 3:
		if (br.Pop()) {
			ret += 1;
			if (br.Pop()) {
				ret += 1;
			}
		}
		break;
	case 4:
		if (br.Pop()) {
			ret += 2;
		}
		if (br.Pop()) {
			ret += 1;
		}
		break;
	case 5:
		if (br.Pop()) {
			ret += 2;
			if (br.Pop()) {
				ret += 1;
				if (br.Pop()) {
					ret += 1;
				}
			}
		}else {
			if (br.Pop()) {
				ret += 1;
			}
		}
		break;
	case 6:
		if (br.Pop()) {
			ret += 2;
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}else {
			if (br.Pop()) {
				ret += 1;
			}
		}
		break;
	case 7:
		if (br.Pop()) {
			ret += 3;
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}else {
			if (br.Pop()) {
				ret += 1;
				if (br.Pop()) {
					ret += 1;
				}
			}
		}
		break;
	case 8:
		if (br.Pop()) {
			ret += 4;
		}
		if (br.Pop()) {
			ret += 2;
		}
		if (br.Pop()) {
			ret += 1;
		}
		break;
	case 9:
		if (br.Pop()) {
			ret += 4;
			if (br.Pop()) {
				ret += 2;
				if (br.Pop()) {
					ret += 1;
					if (br.Pop()) {
						ret += 1;
					}
				}
			}else {
				if (br.Pop()) {
					ret += 1;
				}
			}
		}else {
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}
		break;
	case 10:
		if (br.Pop()) {
			ret += 4;
			if (br.Pop()) {
				ret += 2;
				if (br.Pop()) {
					ret += 2;
				}
				if (br.Pop()) {
					ret += 1;
				}
			}else {
				if (br.Pop()) {
					ret += 1;
				}
			}
		}else {
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}
		break;
	case 11:
		if (br.Pop()) {
			ret += 4;
			bool b1 = br.Pop();
			if (b1) {
				ret += 3;
			}
			bool b2 = br.Pop();
			if (b2) {
				ret += 1;
				if (b1) {
					ret += 1;
				}
			}
			if (b1 || b2) {
				if (br.Pop()) {
					ret += 1;
				}
			}
		}else {
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}
		break;
	case 12:
		if (br.Pop()) {
			ret += 4;
			if (br.Pop()) {
				ret += 4;
			}
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}else {
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}
		break;
	case 13:
		if (br.Pop()) {
			ret += 5;
			if (br.Pop()) {
				ret += 4;
			}
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}else {
			bool b1 = br.Pop();
			if (b1) {
				ret += 2;
			}
			bool b2 = br.Pop();
			if (b2) {
				ret += 1;
			}
			if (b1 && b2) {
				if (br.Pop()) {
					ret += 1;
				}
			}
		}
		break;
	case 14:
		if (br.Pop()) {
			ret += 6;
			if (br.Pop()) {
				ret += 4;
			}
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}else {
			if (br.Pop()) {
				ret += 2;
				if (br.Pop()) {
					ret += 2;
				}
				if (br.Pop()) {
					ret += 1;
				}
			}else {
				if (br.Pop()) {
					ret += 1;
				}
			}
		}
		break;
	case 15:
		if (br.Pop()) {
			ret += 7;
			if (br.Pop()) {
				ret += 4;
			}
			if (br.Pop()) {
				ret += 2;
			}
			if (br.Pop()) {
				ret += 1;
			}
		}else {
			bool b1 = br.Pop();
			if (b1) {
				ret += 3;
			}
			bool b2 = br.Pop();
			if (b2) {
				ret += 1;
				if (b1) {
					ret += 1;
				}
			}
			if (b1 || b2) {
				if (br.Pop()) {
					ret += 1;
				}
			}
		}
		break;
	case 16:
		if (br.Pop()) {
			ret += 8;
		}
		if (br.Pop()) {
			ret += 4;
		}
		if (br.Pop()) {
			ret += 2;
		}
		if (br.Pop()) {
			ret += 1;
		}
		break;
	}
	assert(ret >= 1 && ret <= maxNum);
	return ret;
}

void decodeFills(
	BitReader& br,
	std::vector<FillInfo>& fills,
	uint8_t len1,	// s•ûŒü‚Ì’·‚³
	uint8_t len2	// —ñ•ûŒü‚Ì’·‚³
	)
{
	uint8_t maxLen = decodeNum(br, len2);
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
			offset = decodeNum(br, len2) - 1;
			if (len2-offset == 1) {
				fillLen = 1;
			}else {
				assert(len2 >= offset);
				fillLen = decodeNum(br, std::min(maxLen, (uint8_t)(len2-offset)));
			}
		}else {
			uint8_t diff = len2 - col;
			if (diff == 1) {
				offset = 0;
				fillLen = 1;
			}else {
				offset = decodeNum(br, diff) - 1;
				if (len2-(col+offset) == 1) {
					fillLen = 1;
				}else {
					fillLen = decodeNum(br, std::min(maxLen, (uint8_t)(len2-(col+offset))));
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
	uint8_t x = decodeNum(br, 16) - 1;
	uint8_t y = decodeNum(br, 16) - 1;
	uint8_t w = decodeNum(br, 16-x);
	uint8_t h = decodeNum(br, 16-y);
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

#else

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

#endif
