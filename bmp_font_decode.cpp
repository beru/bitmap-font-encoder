#include "bmp_font_decode.h"

#include "bit_reader.h"
#include "misc.h"
#include "integer_coding.h"

namespace {

void decodeHorizontalFills(
	BitReader& br,
	Array2D<uint8_t>& values
	)
{
	const uint8_t h = values.GetHeight();
	const uint8_t w = values.GetWidth();
	
	// decode line flags
	uint32_t lineFlags = 0;
	for (uint8_t i=0; i<h; ++i) {
		lineFlags |= br.Pop() << i;
	}
	if (!lineFlags) {
		return;
	}
	uint8_t maxLen = integerDecode_CBT(br, w) + 2;
	uint8_t row = ntz(lineFlags);
	uint8_t col = 0;
	do {
		if (col != 0) {
			if (!br.Pop()) {
				row += ntz(lineFlags >> (row+1)) + 1;
				if (row >= h) {
					break;
				}
				col = 0;
			}
		}
		uint8_t remain = w - col;
		if (remain <= 2) {
			// fillLen = 2
			values[row][col] = 1;
			values[row][col+1] = 1;
			col += 2;
		}else {
			uint8_t offset = integerDecode_CBT(br, remain);
			col += offset;
			if (w - col <= 2) {
				// fillLen = 2
				values[row][col] = 1;
				values[row][col+1] = 1;
				col += 2;
			}else {
				uint8_t len = integerDecode_CBT(br, std::min(maxLen, (uint8_t)(w-col))) + 2;
				for (uint8_t i=0; i<len; ++i) {
					values[row][col+i] = 1;
				}
				col += len;
			}
		}
		++col;
		if (col >= w) {
			if (row == h-1) {
				++row;
			}else {
				row += ntz(lineFlags >> (row+1)) + 1;
			}
			col = 0;
		}
	} while (row < h);
}

void decodeVerticalFills(
	BitReader& br,
	Array2D<uint8_t>& values
	)
{
	const uint8_t h = values.GetHeight();
	const uint8_t w = values.GetWidth();
	
	// decode line flags
	uint32_t lineFlags = 0;
	for (uint8_t i=0; i<w; ++i) {
		lineFlags |= br.Pop() << i;
	}
	if (!lineFlags) {
		return;
	}
	const uint8_t firstCol = ntz(lineFlags);
	uint16_t availHeights[16] = {0};
	uint8_t maxAvailHeight = 0;
	for (uint8_t x=0; x<w; ++x) {
		if (lineFlags & (1<<x)) {
			uint8_t ah = h;
			for (uint8_t y=0; y<h; ++y) {
				ah -= values[y][x];
			}
			availHeights[x] = ah;
			maxAvailHeight = std::max(maxAvailHeight, ah);
		}
	}
	uint8_t maxLen = integerDecode_CBT(br, maxAvailHeight) + 1;
	uint32_t fillFlags = 0;
	uint8_t row = 0;
	uint8_t col = firstCol;
	do {
		const uint8_t availHeight = availHeights[col];
		uint8_t remain = availHeight - row;
		if (remain == 1) {
			fillFlags |= 1 << row;
		}else {
			uint8_t offset = integerDecode_CBT(br, remain);
			row += offset;
			if (availHeight - row == 1) {
				// fillLen = 1
				fillFlags |= 1 << row;
				row += 1;
			}else {
				uint8_t len = integerDecode_CBT(br, std::min(maxLen, (uint8_t)(availHeight-row))) + 1;
				for (uint8_t i=0; i<len; ++i) {
					fillFlags |= 1 << (row+i);
				}
				row += len;
			}
		}
		++row;
		if (row >= availHeight || !br.Pop()) {
			uint8_t fi = 0;
			for (uint8_t i=0; i<h; ++i) {
				if (values[i][col]) {
					continue;
				}
				if (fillFlags & (1<<fi++)) {
					values[i][col] = 1;
				}
			}
			fillFlags = 0;
			col += ntz(lineFlags >> (col+1)) + 1;
			row = 0;
		}
	} while (col < w);
}

void decodeFills(
	BitReader& br,
	Array2D<uint8_t>& values
	)
{
	decodeHorizontalFills(br, values);
	decodeVerticalFills(br, values);
}

} // namespace anonymous

void Decode(BitReader& br, BitmapFont& bf, const BmpFontHeader& fontInfo)
{
	uint8_t recX = integerDecode_Alpha(br);
	uint8_t recY = integerDecode_Alpha(br);
	
	uint8_t recW = integerDecode_Alpha(br);
	uint8_t recH = integerDecode_Alpha(br);
	
	if (recX == 15) {
		recX = 0;
	}else {
		++recX;
	}
	
	bf.x_ = recX + fontInfo.minX;
	bf.y_ = recY + fontInfo.minY;
	bf.w_ = fontInfo.maxW - recX - recW;
	bf.h_ = fontInfo.maxH - recY - recH;
	
	bf.Init(bf.w_, bf.h_);
	
	decodeFills(br, bf.values_);
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

