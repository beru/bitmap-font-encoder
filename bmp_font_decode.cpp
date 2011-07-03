#include "bmp_font_decode.h"

#include "bit_reader.h"
#include "misc.h"
#include "integer_coding.h"

struct FillInfo
{
	uint8_t p1;
	uint8_t p2;
	uint8_t len;
};

void decodeFills(
	BitReader& br,
	std::vector<FillInfo>& fills,
	uint8_t len1,	// s•ûŒü‚Ì’·‚³
	uint8_t len2	// —ñ•ûŒü‚Ì’·‚³
	)
{
	uint16_t lineFlags = 0;
	for (uint8_t i=0; i<len1; ++i) {
		lineFlags |= br.Pop() << i;
	}
	if (!lineFlags) {
		return;
	}
	uint8_t maxLen = integerDecode_CBT(br, len2)+1;
	uint8_t row = ntz(lineFlags);
	
	uint8_t col = 0;
	do {
		uint8_t offset;
		uint8_t fillLen;
		if (col == 0) {
			offset = integerDecode_CBT(br, len2);
			if (len2-offset == 1) {
				fillLen = 1;
			}else {
				assert(len2 >= offset);
				fillLen = integerDecode_CBT(br, std::min(maxLen, (uint8_t)(len2-offset))) + 1;
			}
		}else {
			bool b = br.Pop();
			if (!b) {
				row += ntz(lineFlags >> (row+1)) + 1;
				col = 0;
				continue;
			}
			uint8_t diff = len2 - col;
			if (diff == 1) {
				offset = 0;
				fillLen = 1;
			}else {
				offset = integerDecode_CBT(br, diff);
				if (len2-(col+offset) == 1) {
					fillLen = 1;
				}else {
					fillLen = integerDecode_CBT(br, std::min(maxLen, (uint8_t)(len2-(col+offset)))) + 1;
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
			if (row == len1-1) {
				++row;
			}else {
				row += ntz(lineFlags >> (row+1)) + 1;
			}
			col = 0;
		}
	}while (row < len1);
}

void Decode(BitReader& br, BitmapFont& bf, const BmpFontHeader& fontInfo)
{
	uint8_t recX = integerDecode_Alpha(br);
	uint8_t recY = integerDecode_Alpha(br);
	
	uint8_t recW = integerDecode_Alpha(br);
	uint8_t recH = integerDecode_Alpha(br);
	
	if (recX == 15) {
		recX = 0;
	}

	bf.x_ = recX + fontInfo.minX;
	bf.y_ = recY + fontInfo.minY;
	bf.w_ = fontInfo.maxW - recX - recW;
	bf.h_ = fontInfo.maxH - recY - recH;
	
	bf.Init(bf.w_, bf.h_);
	
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	decodeFills(br, hFills, bf.h_, bf.w_);
	decodeFills(br, vFills, bf.w_, bf.h_);
	
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

bool DecodeHeader(class BitReader& br, BmpFontHeader& header)
{
	popBits(br, header.characterCount);
	uint16_t code = 0;
	popBits(br, code);
	uint16_t* codes = header.characterCodes;
	codes[0] = code;
	uint16_t diff;
	for (uint16_t i=1; i<header.characterCount; ++i) {
		diff = integerDecode_Delta(br);
		code += diff + 1;
		codes[i] = code;
	}
	header.minX = integerDecode_CBT(br, 16);
	header.minY = integerDecode_CBT(br, 16);
	header.maxW = integerDecode_CBT(br, 16) + 1;
	header.maxH = integerDecode_CBT(br, 16) + 1;
	return true;
}

