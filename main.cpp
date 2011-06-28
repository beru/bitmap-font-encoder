
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
	
	std::string ret;
	ret.resize(200);
	{
		const size_t hBytes = (header.FONTBOUNDINGBOX[0] + 7) / 8;
#if 1
		BitWriter bitWriter;
		BitReader bitReader;
		uint8_t dest[4096] = {0};
		bitWriter.Set(&dest[0]);
		bitReader.Set(&dest[0]);
		FILE* of = fopen("encoded.txt", "wb");	// it'll be huge
//		const wchar_t* str = L"入";
		const wchar_t* str = L"前後通信非入出力確認間初期防無効禁止容注意取扱説明書極性番的差現使用害切替損傷最大動押増加項数値点滅電源圧流気接続能表示設定誤操作等荷演算解除総重量軸画字絹風袋線兆億万千百十九八七六五四三二一＝】計測機器端麗辛口";
//		const wchar_t* str = L"吾輩（わがはい）は猫である。名前はまだ無い。";
//		const wchar_t* str = L"ぁあぃいぅうぇえぉおかがきぎくぐけげこごさざしじすずせぜそぞただちぢっつづてでとどなにぬねのはばぱひびぴふぶぷへべぺほぼぽまみむめもゃやゅゆょよらりるれろゎわゐゑをん";
//		const wchar_t* str = L"ァアィイゥウェエォオカガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツヅテデトドナニヌネノハバパヒビピフブプヘベペホボポマミムメモャヤュユョヨラリルレロヮワヰヱヲンヴヵヶ";
		BitmapFont bmpFont;
		for (size_t i=0; i<wcslen(str); ++i) {
			wchar_t c = str[i];
			uint16_t unicode = c;
			uint16_t jis = CodeConvert::unicode_to_jis(unicode);
			assert(jis);
			uint16_t idx = encoding_idx_table[jis];
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
			bmpFont.Compact();
			fputs(bmpFont.Dump().c_str(), of);
			size_t oldNBits = bitWriter.GetNBits();
			fputs(
				Encode(bmpFont, bitWriter).c_str()
			, of)
			;
			fprintf(of, "num of bits : %d\r\n", bitWriter.GetNBits()-oldNBits);
		}

		bmpFont.Init(1,1);
		for (size_t i=0; i<wcslen(str); ++i) {
			Decode(bmpFont, bitReader);
			std::string s = bmpFont.Dump();
			fputs(s.c_str(), of);
			fputs("\r\n", of);
		}
		fprintf(of, "total num of bits : %d\r\n", bitWriter.GetNBits());
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
		int hoge = 0;
	}
	return 0;
}

