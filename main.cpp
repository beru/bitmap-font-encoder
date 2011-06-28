
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
//		const wchar_t* str = L"��";
		const wchar_t* str = L"�O��ʐM����o�͊m�F�ԏ����h�����֎~�e���ӎ戵�������ɐ��ԓI�����g�p�Q�֑ؑ����ő哮�����������l�_�œd�������C�ڑ��\�\���ݒ�둀�쓙�׉��Z�������d�ʎ��掚�����ܐ���������S�\�㔪���Z�܎l�O��ꁁ�z�v���@��[��h��";
//		const wchar_t* str = L"��y�i�킪�͂��j�͔L�ł���B���O�͂܂������B";
//		const wchar_t* str = L"�����������������������������������������������������������������������ÂĂłƂǂȂɂʂ˂̂͂΂ςЂт҂ӂԂՂւׂ؂قڂۂ܂݂ނ߂��������������������";
//		const wchar_t* str = L"�@�A�B�C�D�E�F�G�H�I�J�K�L�M�N�O�P�Q�R�S�T�U�V�W�X�Y�Z�[�\�]�^�_�`�a�b�c�d�e�f�g�h�i�j�k�l�m�n�o�p�q�r�s�t�u�v�w�x�y�z�{�|�}�~����������������������������������������������";
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

