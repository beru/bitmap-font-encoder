
#include <stdio.h>
#include <vector>
#include "bdf.h"
#include "code_convert.h"
#include "bmp_font_encoder.h"

size_t GetFileSize(FILE* file)
{
	fseek(file, 0, SEEK_END);
	int length = ftell(file);
	fseek(file, 0, SEEK_SET);
	return length;
}

// encoding code to segment index
uint16_t encoding_idx_table[0xffff];

void printByteBits(FILE* f, uint8_t b)
{
	for (size_t i=0; i<8; ++i) {
		bool bit = (b >> (8-1-i)) & 1;
		fputc((bit ? '#':' '), f);
	}

}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("usage : imacat2aa bdf_filename\r\n");
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
	
	{
		const size_t hBytes = (header.FONTBOUNDINGBOX[0] + 7) / 8;
#if 1
		FILE* of = fopen("encoded.txt", "wb");	// it'll be huge
//		const wchar_t* str = L"吾輩（わがはい）は猫である。名前はまだ無い。";
		const wchar_t* str = L"あ";
		BitmapFontEncoder encoder;
		for (size_t i=0; i<wcslen(str); ++i) {
			wchar_t c = str[i];
			uint16_t unicode = c;
			uint16_t jis = CodeConvert::unicode_to_jis(unicode);
			uint16_t idx = encoding_idx_table[jis];
			const BDF::CharacterSegment& seg = segments[idx];
			uint32_t offset = idx * segDataSize;
			uint8_t w = seg.BBX[0];
			uint8_t h = seg.BBX[1];
			encoder.Init(seg.BBX[0], seg.BBX[1]);
			for (size_t y=0; y<segDataSize/hBytes; ++y) {
				for (size_t hi=0; hi<hBytes; ++hi) {
					uint8_t b = data[offset + hBytes*y + hi];
					for (size_t x=0; x<8; ++x) {
						if ((b >> (7-x)) & 1) {
							encoder.FillPixel(8*hi+x, y);
						}
					}
				}
			}
			encoder.Compact();
			fputs(encoder.Dump().c_str(), of);
			fputs(encoder.BreakPixels().c_str(), of);
		}
		fclose(of);
#else
		FILE* f = fopen("imacat.txt", "rb");		// http://www.aozora.gr.jp/cards/000148/files/789_14547.html ( save in utf-16le )
		FILE* of = fopen("imacat_aa.txt", "wb");	// it'll be huge
		wint_t c;
		while ((c = fgetwc(f)) != 0xffff) {
			uint16_t unicode = c;
			uint16_t jis = CodeConvert::unicode_to_jis(unicode);
			uint16_t idx = encoding_idx_table[jis];
			const BDF::CharacterSegment& seg = segments[idx];
			uint32_t offset = idx * segDataSize;
			for (size_t j=0; j<segDataSize; j+=hBytes) {
	//			printf("%02X", data[offset+j]);
				printByteBits(of, data[offset+j]);
				printByteBits(of, data[offset+j+1]);
				fprintf(of, "\r\n");
			}
			fprintf(of, "\r\n");
	//		printf("%d %d\r\n", seg.BBX[0], seg.BBX[1]);
		}
		fclose(f);
		fclose(of);
#endif
	}
	return 0;
}

