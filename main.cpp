
#include <stdio.h>
#include <vector>
#include "bdf.h"
#include "code_convert.h"
#include "bmp_font.h"
#include "bit_writer.h"
#include "bit_reader.h"
#include "bmp_font_encode.h"
#include "bmp_font_decode.h"
#include <algorithm>
#include "chc.h"
#include "integer_coding.h"

// encoding code to segment index
uint16_t encoding_idx_table[0xffff];

void loadBDFdata(
	BitmapFont& bmpFont,
	uint16_t idx,
	const std::vector<BDF::CharacterSegment>& segments,
	size_t segDataSize,
	size_t hBytes,
	const std::vector<uint8_t>& data
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
	const std::vector<uint8_t>& data
	)
{
	uint16_t unicode = c;
	uint16_t jis = CodeConvert::unicode_to_jis(unicode);
	assert(jis);
	uint16_t idx = encoding_idx_table[jis];
	loadBDFdata(bmpFont, idx, segments, segDataSize, hBytes, data);
}

uint32_t g_dist[17][33][33];


void chcTest()
{
	size_t totalSize_Old = 0;
	size_t totalSize_New = 0;
	
	for (uint8_t t=0; t<=7; ++t) {
		for (uint8_t i=0; i<17; ++i) {
			uint8_t entryLen = 0;
			CHC_Entry entries[17] = {0};
			for (uint8_t j=0; j<17; ++j) {
				uint32_t v = g_dist[t][i][j];
				if (v == 0) {
					break;
				}
				assert(j <= 16);
				CHC_Entry& e = entries[j];
				e.code = j;
				e.cnt = v;
				++entryLen;
			}
			if (entryLen < 4) {
				continue;
			}
			BuildCanonicalHuffmanCodes(entries, entryLen);
			
			for (uint8_t j=0; j<entryLen; ++j) {
				const CHC_Entry& e = entries[j];
				if (t == 7) {
					totalSize_Old += e.cnt * (e.code+1);
				}else {
					totalSize_Old += e.cnt * calcIntegerEncodedLength_CBT(j, entryLen);
				}
				totalSize_New += e.cnt * e.bitLen;
			}

			int hoge = 0;
		}
	}
}

void countBoxInfo(const BitmapFont& bf, std::vector<BoxInfo>& boxInfos)
{
	bool bFound = false;
	for (size_t i=0; i<boxInfos.size(); ++i) {
		BoxInfo& bi = boxInfos[i];
		if (1
			&& bi.x == bf.x_
			&& bi.y == bf.y_
			&& bi.w == bf.w_
			&& bi.h == bf.h_
			) {
			++bi.cnt;
			bFound = true;
			break;
		}
	}
	if (!bFound) {
		BoxInfo bi;
		bi.x = bf.x_;
		bi.y = bf.y_;
		bi.w = bf.w_;
		bi.h = bf.h_;
		bi.cnt = 1;
		boxInfos.push_back(bi);
	}
}

uint8_t calcBoxInfoCHCEntries(
	std::vector<BoxInfo>& infos,
	std::vector<CHC_Entry>& entries,
	uint8_t size
	)
{
	std::sort(infos.rbegin(), infos.rend());
	uint16_t cnt = 0;
	for (size_t i=size-1; i<infos.size(); ++i) {
		const BoxInfo& bi = infos[i];
		cnt += bi.cnt;
	}
	BoxInfo& bi = infos[size-1];
	bi.x = 0xFF;
	bi.y = 0xFF;
	bi.w = 0xFF;
	bi.h = 0xFF;
	bi.cnt = cnt;
	std::sort(infos.rbegin(), infos.rend());
	
	entries.resize(size);
	for (size_t i=0; i<size; ++i) {
		CHC_Entry& e = entries[i];
		const BoxInfo& bi = infos[i];
		e.cnt = bi.cnt;
		e.code = i;
		e.bitLen = 0;
		e.chc = 0;
	}
	uint8_t idx = -1;
	BuildCanonicalHuffmanCodes(&entries[0], size);
	for (size_t i=0; i<size; ++i) {
		const CHC_Entry& e = entries[i];
		BoxInfo& bi = infos[i];
		bi.bitLen = e.bitLen;
		bi.chc = e.chc;

		if (1
			&& bi.x == 0xFF
			&& bi.y == 0xFF
			&& bi.w == 0xFF
			&& bi.h == 0xFF
			) {
			idx = i;
		}
	}
	return idx;
}

void encodeCharacters(
	const BDF::Header& header,
	const std::vector<BDF::CharacterSegment>& segments,
	size_t segDataSize,
	const std::vector<uint8_t>& bitmapData,
	BitWriter& bitWriter,
	FILE* of,
	uint16_t strLen,
	const uint16_t* idxs
	)
{
	fprintf(of, "string length : %d\r\n", strLen);
	const size_t hBytes = (header.FONTBOUNDINGBOX[0] + 7) / 8;
	
	BitmapFont bmpFont;
	BmpFontHeader bmpFontHeader;
	uint8_t minX = -1;
	uint8_t minY = -1;
	uint8_t maxW = 0;
	uint8_t maxH = 0;
	std::vector<uint16_t> strs(strLen);
	for (size_t i=0; i<strLen; ++i) {
		uint16_t idx = idxs[i];
		strs[i] = segments[idx].ENCODING;
		loadBDFdata(bmpFont, i, segments, segDataSize, hBytes, bitmapData);
		bmpFont.Compact();
		assert(bmpFont.w_ <= 16);
		assert(bmpFont.h_ <= 16);
		minX = std::min(minX, bmpFont.x_);
		minY = std::min(minY, bmpFont.y_);
		maxW = std::max(maxW, bmpFont.w_);
		maxH = std::max(maxH, bmpFont.h_);
	}
	bmpFontHeader.minX = minX;
	bmpFontHeader.minY = minY;
	bmpFontHeader.maxW = maxW;
	bmpFontHeader.maxH = maxH;
	bmpFontHeader.characterCount = strLen;
	bmpFontHeader.characterCodes = &strs[0];
	fputs(EncodeHeader(bitWriter, bmpFontHeader).c_str(), of);

	std::vector<BoxInfo> boxInfos;
	for (size_t i=0; i<strLen; ++i) {
		uint16_t idx = idxs[i];
		loadBDFdata(bmpFont, idx, segments, segDataSize, hBytes, bitmapData);
		bmpFont.Compact();
		countBoxInfo(bmpFont, boxInfos);
	}
	size_t chcEntryCount = 16;
	std::vector<CHC_Entry> chcEntries;
	uint8_t regIdx = calcBoxInfoCHCEntries(boxInfos, chcEntries, chcEntryCount);
	EncodeBoxTable(bitWriter, &boxInfos[0], chcEntryCount, regIdx, bmpFontHeader);
	
	for (size_t i=0; i<strLen; ++i) {
		uint16_t idx = idxs[i];
		loadBDFdata(bmpFont, idx, segments, segDataSize, hBytes, bitmapData);
		bmpFont.Compact();
		fputs(bmpFont.Dump().c_str(), of);
		fputs("\r\n", of);
		size_t oldNBits = bitWriter.GetNBits();
//TRACE("%d\r\n", bitWriter.GetNBits());

		Encode(bitWriter, bmpFont, bmpFontHeader, &boxInfos[0], chcEntryCount, regIdx);
//			fprintf(of, "num of bits : %8d\r\n", bitWriter.GetNBits()-oldNBits);
	}
}

void decodeCharacters(const uint8_t* pData, size_t bytes, FILE* f)
{
	BitReader bitReader;
	bitReader.Set(pData);

	BmpFontHeader header = {0};
	uint16_t codes[8192] = {0};
	header.characterCodes = codes;
	if (!DecodeHeader(bitReader, header)) {
		return;
	}
	BitmapFont bf;
	for (uint16_t i=0; i<header.characterCount; ++i) {
TRACE("%d\r\n", bitReader.GetTotalCounter());
		Decode(bitReader, bf, header);
		fputs(bf.Dump().c_str(), f);
		fputs("\r\n", f);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("usage : bitmap_font_encoder bdf_filename\r\n");
		return 1;
	}
	
	FILE* of = fopen("result.txt", "wb");	// it'll be huge
	// encode
	{
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
		std::vector<uint8_t> bitmapData(header.CHARS * segDataSize);
		BDF::ReadCharacterSegments(segStart, bytes.size() - (segStart - &bytes[0]), header, &segments[0], &bitmapData[0]);
		
		// initialize code convertion routine
		if (!CodeConvert::Init()) {
			return 1;
		}
		for (size_t i=0; i<header.CHARS; ++i) {
			const BDF::CharacterSegment& seg = segments[i];
			encoding_idx_table[ seg.ENCODING ] = i;
		}
		BitWriter bitWriter;
		std::vector<uint8_t> dest(1024*1024);
		bitWriter.Set(&dest[0]);

		std::vector<uint16_t> idxs;
#if 1
		const size_t strLen = header.CHARS;
		idxs.resize(strLen);
		for (size_t i=0; i<strLen; ++i) {
			idxs[i] = i;
		}
#else
//		const wchar_t* str = L"Šm"; // ÝŠÔ
//		const wchar_t* str = L"M”ñlo—ÍŠÔŽ~ˆÓà–¾‘“I·Œ»—pØ‰Ÿ‰Á’l“_“dˆ³‹C”\Ý“™";
//		const wchar_t* str = L"Œá”yi‚í‚ª‚Í‚¢j‚Í”L‚Å‚ ‚éB–¼‘O‚Í‚Ü‚¾–³‚¢B";
#if 1
		const wchar_t* str = 
			L"‘OŒã’ÊM”ñ“üo—ÍŠm”FŠÔ‰Šú–h–³Œø‹ÖŽ~—e’ˆÓŽæˆµà–¾‘‹É«”Ô“I·Œ»Žg—pŠQØ‘Ö‘¹Å‘å“®‰Ÿ‘‰Á€”’l“_–Å“dŒ¹ˆ³—¬‹CÚ‘±”\•\Ž¦Ý’èŒë‘€ì“™‰×‰‰ŽZ‰ðœ‘d—ÊŽ²‰æŽšŒ¦•—‘ÜüŒv‘ª‹@Ší’[—íhŒû"
			L"‚Ÿ‚ ‚¡‚¢‚£‚¤‚¥‚¦‚§‚¨‚©‚ª‚«‚¬‚­‚®‚¯‚°‚±‚²‚³‚´‚µ‚¶‚·‚¸‚¹‚º‚»‚¼‚½‚¾‚¿‚À‚Á‚Â‚Ã‚Ä‚Å‚Æ‚Ç‚È‚É‚Ê‚Ë‚Ì‚Í‚Î‚Ï‚Ð‚Ñ‚Ò‚Ó‚Ô‚Õ‚Ö‚×‚Ø‚Ù‚Ú‚Û‚Ü‚Ý‚Þ‚ß‚à‚á‚â‚ã‚ä‚å‚æ‚ç‚è‚é‚ê‚ë‚ì‚í‚î‚ï‚ð‚ñ"
			L"ƒ@ƒAƒBƒCƒDƒEƒFƒGƒHƒIƒJƒKƒLƒMƒNƒOƒPƒQƒRƒSƒTƒUƒVƒWƒXƒYƒZƒ[ƒ\ƒ]ƒ^ƒ_ƒ`ƒaƒbƒcƒdƒeƒfƒgƒhƒiƒjƒkƒlƒmƒnƒoƒpƒqƒrƒsƒtƒuƒvƒwƒxƒyƒzƒ{ƒ|ƒ}ƒ~ƒ€ƒƒ‚ƒƒƒ„ƒ…ƒ†ƒ‡ƒˆƒ‰ƒŠƒ‹ƒŒƒƒŽƒƒƒ‘ƒ’ƒ“ƒ”ƒ•ƒ–"
			L"BDAC’›‰­–œç•S\‹ã”ªŽµ˜ZŒÜŽlŽO“ñˆê‚O‚P‚Q‚R‚S‚T‚U‚V‚W‚X•ijopyzƒ„‚`‚a‚b‚c‚d‚e‚f‚g‚h‚i‚j‚k‚l‚m‚n‚o‚p‚q‚r‚s‚t‚u‚v‚w‚x‚y‚‚‚‚ƒ‚„‚…‚†‚‡‚ˆ‚‰‚Š‚‹‚Œ‚‚Ž‚‚‚‘‚’‚“‚”‚•‚–‚—‚˜‚™‚š"
//			L"Š@ŠAŠBŠCŠDŠEŠFŠGŠHŠIŠJŠKŠLŠMŠNŠOŠPŠQŠRŠSŠTŠUŠVŠWŠXŠYŠZŠ[Š\Š]Š^Š_Š`ŠaŠbŠcŠdŠeŠfŠgŠhŠiŠjŠkŠlŠmŠnŠoŠpŠqŠrŠsŠtŠuŠvŠwŠxŠyŠzŠ{Š|Š}Š~Š€ŠŠ‚ŠƒŠ„Š…Š†Š‡ŠˆŠ‰ŠŠŠ‹ŠŒŠŠŽŠŠŠ‘Š’Š“Š”Š•Š–Š—Š˜Š™ŠšŠ›ŠœŠŠž"
		;
#endif
		const uint16_t strLen = wcslen(str);
		idxs.resize(strLen);
		for (uint16_t i=0; i<strLen; ++i) {
			uint16_t unicode = str[i];
			uint16_t jis = CodeConvert::unicode_to_jis(unicode);
			assert(jis);
			uint16_t idx = encoding_idx_table[jis];
			idxs[i] = idx;
		}
		std::sort(idxs.begin(), idxs.end());
#endif

		encodeCharacters(header, segments, segDataSize, bitmapData, bitWriter, of, strLen, &idxs[0]);
		size_t totalBits = bitWriter.GetNBits();
		fprintf(of, "total num of bits : %d\r\n", totalBits);
		fprintf(of, "total num of bytes : %d\r\n", totalBits/8);
		fprintf(of, "total num of killo bytes : %f\r\n", totalBits/8.0/1024);
		int hoge = 0;

		FILE* f = fopen("output.bin", "wb");
		fwrite(&dest[0], 1, bitWriter.GetNBytes(), f);
		fclose(f);
	}
	
	chcTest();
	
	// decode test
	if (0) {
		FILE* f = fopen("output.bin", "rb");
		size_t fileSize = GetFileSize(f);
		std::vector<uint8_t> data(fileSize);
		fread(&data[0], 1, fileSize, f);
		fclose(f);
		
		decodeCharacters(&data[0], data.size(), of);
	}
	fclose(of);

	return 0;
}

