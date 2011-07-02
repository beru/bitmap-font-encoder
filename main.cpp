
#include <stdio.h>
#include <vector>
#include "bdf.h"
#include "code_convert.h"
#include "bmp_font.h"
#include "bmp_font_encode.h"
#include "bit_writer.h"
#include "bit_reader.h"

// encoding code to segment index
uint16_t encoding_idx_table[0xffff];

void loadBDFdata(
	BitmapFont& bmpFont,
	uint16_t idx,
	const std::vector<BDF::CharacterSegment>& segments,
	size_t segDataSize,
	size_t hBytes,
	std::vector<uint8_t>& data
	)
{
	const BDF::CharacterSegment& seg = segments[idx];
	uint32_t offset = idx * segDataSize;
	uint8_t w = seg.BBX[0];
	uint8_t h = seg.BBX[1];
	bmpFont.Init(seg.BBX[0], seg.BBX[1]);
	for (size_t y=0; y<segDataSize/hBytes; ++y) {
		for (size_t hi=0; hi<hBytes; ++hi) {
			uint8_t b = data[offset + hBytes*y + hi];
			for (size_t x=0; x<8; ++x) {
				if ((b >> (7-x)) & 1) {
					bmpFont.FillPixel(8*hi+x, y);
				}
			}
		}
	}

}

void loadUnicodeBDFdata(
	BitmapFont& bmpFont,
	wchar_t c,
	const std::vector<BDF::CharacterSegment>& segments,
	size_t segDataSize,
	size_t hBytes,
	std::vector<uint8_t>& data
	)
{
	uint16_t unicode = c;
	uint16_t jis = CodeConvert::unicode_to_jis(unicode);
	assert(jis);
	uint16_t idx = encoding_idx_table[jis];
	loadBDFdata(bmpFont, idx, segments, segDataSize, hBytes, data);
}

uint16_t g_dist[16][16];

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("usage : bitmap_font_encoder bdf_filename\r\n");
		return 1;
	}

	// load bdf file ( tested with shnmk16.bdf )
	std::vector<char> bytes;
	{
		FILE* f = fopen(argv[1], "rb");
		if (!f) {
			printf("failed to open file\r\n");
			return 1;
		}
		size_t fileSize = GetFileSize(f);
		bytes.resize(fileSize);
		fread(&bytes[0], 1, fileSize, f);
		fclose(f);
	}
	
	BDF::Header header;
	const char* segStart = BDF::ReadHeader(&bytes[0], bytes.size(), header);
	if (segStart == 0) {
		printf("character segments not found.\n");
		return 1;
	}

	std::vector<BDF::CharacterSegment> segments(header.CHARS);
	size_t segDataSize = BDF::CalcSegmentMaxDataSize(header);
	std::vector<uint8_t> data(header.CHARS * segDataSize);
	BDF::ReadCharacterSegments(segStart, bytes.size() - (segStart - &bytes[0]), header, &segments[0], &data[0]);
	
	// initialize code convertion routine
	if (!CodeConvert::Init()) {
		return 1;
	}
	for (size_t i=0; i<header.CHARS; ++i) {
		const BDF::CharacterSegment& seg = segments[i];
		encoding_idx_table[ seg.ENCODING ] = i;
	}
	
	std::string ret;
	ret.resize(200);
	{
		const size_t hBytes = (header.FONTBOUNDINGBOX[0] + 7) / 8;
		BitWriter bitWriter;
		BitReader bitReader;
		std::vector<uint8_t> dest(1024*1024);
		bitWriter.Set(&dest[0]);
		bitReader.Set(&dest[0]);
		FILE* of = fopen("encoded.txt", "wb");	// it'll be huge

		const size_t strLen = header.CHARS;
		fprintf(of, "string length : %d\r\n", strLen);
		
		BitmapFont bmpFont;
		uint8_t minX = -1;
		uint8_t minY = -1;
		uint8_t maxW = 0;
		uint8_t maxH = 0;
		std::vector<uint16_t> strs(strLen);
		for (size_t i=0; i<strLen; ++i) {
			strs[i] = segments[i].ENCODING;
			loadBDFdata(bmpFont, i, segments, segDataSize, hBytes, data);
			bmpFont.Compact();
			assert(bmpFont.w_ <= 16);
			assert(bmpFont.h_ <= 16);
			minX = std::min(minX, bmpFont.x_);
			minY = std::min(minY, bmpFont.y_);
			maxW = std::max(maxW, bmpFont.w_);
			maxH = std::max(maxH, bmpFont.h_);
		}
		fputs(EncodeHeader(bitWriter, strLen, &strs[0], minX, minY, maxW, maxH).c_str(), of);
		for (size_t i=0; i<strLen; ++i) {
			loadBDFdata(bmpFont, i, segments, segDataSize, hBytes, data);
			bmpFont.Compact();
//			fputs(bmpFont.Dump().c_str(), of);
			size_t oldNBits = bitWriter.GetNBits();
//			fputs(
				Encode(bitWriter, bmpFont, minX, minY, maxW, maxH).c_str()
//			, of)
			;
//			fprintf(of, "num of bits : %d\r\n", bitWriter.GetNBits()-oldNBits);
		}

#if 0
		bmpFont.Init(1,1);
		for (size_t i=0; i<wcslen(str); ++i) {
			Decode(bmpFont, bitReader);
			std::string s = bmpFont.Dump();
			fputs(s.c_str(), of);
			fputs("\r\n", of);
		}
#endif
		size_t totalBits = bitWriter.GetNBits();
		fprintf(of, "total num of bits : %d\r\n", totalBits);
		fprintf(of, "total num of bytes : %d\r\n", totalBits/8);
		fprintf(of, "total num of killo bytes : %f\r\n", totalBits/8.0/1024);
		fclose(of);
		int hoge = 0;
	}
	return 0;
}

