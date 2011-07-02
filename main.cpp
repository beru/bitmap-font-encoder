
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
		uint8_t dest[8192*10] = {0};
		bitWriter.Set(&dest[0]);
		bitReader.Set(&dest[0]);
		FILE* of = fopen("encoded.txt", "wb");	// it'll be huge
//		const wchar_t* str = L"�m"; // �݊�
//		const wchar_t* str = L"�M��l�o�͊Ԏ~�Ӑ������I�����p�؉����l�_�d���C�\�ݓ�";
//		const wchar_t* str = L"��y�i�킪�͂��j�͔L�ł���B���O�͂܂������B";
#if 1
		const wchar_t* str = 
			L"�O��ʐM����o�͊m�F�ԏ����h�����֎~�e���ӎ戵�������ɐ��ԓI�����g�p�Q�֑ؑ����ő哮�����������l�_�œd�������C�ڑ��\�\���ݒ�둀�쓙�׉��Z�������d�ʎ��掚�����ܐ��v���@��[��h��"
			L"�����������������������������������������������������������������������ÂĂłƂǂȂɂʂ˂̂͂΂ςЂт҂ӂԂՂւׂ؂قڂۂ܂݂ނ߂��������������������"
			L"�@�A�B�C�D�E�F�G�H�I�J�K�L�M�N�O�P�Q�R�S�T�U�V�W�X�Y�Z�[�\�]�^�_�`�a�b�c�d�e�f�g�h�i�j�k�l�m�n�o�p�q�r�s�t�u�v�w�x�y�z�{�|�}�~����������������������������������������������"
			L"�B�D�A�C��������S�\�㔪���Z�܎l�O���O�P�Q�R�S�T�U�V�W�X�����i�j�o�p�y�z�����`�a�b�c�d�e�f�g�h�i�j�k�l�m�n�o�p�q�r�s�t�u�v�w�x�y����������������������������������������������������"
			L"�@�A�B�C�D�E�F�G�H�I�J�K�L�M�N�O�P�Q�R�S�T�U�V�W�X�Y�Z�[�\�]�^�_�`�a�b�c�d�e�f�g�h�i�j�k�l�m�n�o�p�q�r�s�t�u�v�w�x�y�z�{�|�}�~��������������������������������������������������������������"
		;
#endif
		const size_t strLen = wcslen(str);
		fprintf(of, "string length : %d\r\n", strLen);
		BitmapFont bmpFont;
		uint8_t minX = -1;
		uint8_t minY = -1;
		uint8_t maxW = 0;
		uint8_t maxH = 0;
		for (size_t i=0; i<strLen; ++i) {
			wchar_t c = str[i];
			loadUnicodeBDFdata(bmpFont, c, segments, segDataSize, hBytes, data);
			bmpFont.Compact();
			minX = std::min(minX, bmpFont.x_);
			minY = std::min(minY, bmpFont.y_);
			maxW = std::max(maxW, bmpFont.w_);
			maxH = std::max(maxH, bmpFont.h_);
		}
		fputs(EncodeHeader(bitWriter, minX, minY, maxW, maxH).c_str(), of);
		for (size_t i=0; i<strLen; ++i) {
			wchar_t c = str[i];
			loadUnicodeBDFdata(bmpFont, c, segments, segDataSize, hBytes, data);
			bmpFont.Compact();
			fputs(bmpFont.Dump().c_str(), of);
			size_t oldNBits = bitWriter.GetNBits();
			fputs(
				Encode(bitWriter, bmpFont, minX, minY, maxW, maxH).c_str()
			, of)
			;
			fprintf(of, "num of bits : %d\r\n", bitWriter.GetNBits()-oldNBits);
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
		fprintf(of, "total num of bits : %d\r\n", bitWriter.GetNBits());
		fclose(of);
		int hoge = 0;
	}
	return 0;
}

