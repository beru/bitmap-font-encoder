#include "bmp_font_encode.h"

#include <algorithm>
#include "bit_writer.h"
#include "misc.h"
#include "integer_coding.h"

struct FillInfo
{
	uint8_t p1;
	uint8_t p2;
	uint8_t len;
};

bool operator < (const FillInfo& lhs, const FillInfo& rhs)
{
	return lhs.p1 < rhs.p1;
}

static
void buildVerticalCommands(
	BitWriter& bw,
	std::vector<std::string>& cmds,
	const std::vector<FillInfo>& fills,
	uint8_t len1,
	const uint8_t* len2s
	)
{
	if (fills.size() == 0) {
		// 全部改行！
		for (uint8_t i=0; i<len1; ++i) {
			cmds.push_back("row 0");
			bw.Push(false);
		}
		return;
	}
	
	// 空行かどうかの記録
	uint16_t lineFlags = 0;
	uint8_t maxLen = 1;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		maxLen = std::max(maxLen, fi.len);
		lineFlags |= 1 << fi.p1;
	}
	for (uint8_t i=0; i<len1; ++i) {
		bw.Push(lineFlags & (1<<i));
		if (lineFlags & (1<<i)) {
			cmds.push_back("row 1");
		}else {
			cmds.push_back("row 0");
		}
	}
	// 最大線長の記録
	char buff[32];
	sprintf(buff, "max line length : %d", maxLen);
	cmds.push_back(buff);
	uint8_t maxLen2 = 0;
	// 縦面の場合の事も考えて最大線長を再度収集する
	for (uint8_t i=0; i<len1; ++i) {
		if (lineFlags & (1<<i)) {
			maxLen2 = std::max(maxLen2, len2s[i]);
		}
	}
	integerEncode_CBT(bw, maxLen-1, maxLen2);
	
	uint8_t col = 0;
	uint8_t row = fills[0].p1;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		if (row != fi.p1) {
			if (col < len2s[row]) {
				bw.Push(false);
				cmds.push_back("next line");
			}
			col = 0;
		}
		row = fi.p1;
		uint8_t len2 = len2s[row];
		char buff[32];
		uint8_t offset = fi.p2 - col;
		sprintf(buff, "fill %d %d", offset, fi.len);
		cmds.push_back(buff);
		
		if (col != 0) {
			bw.Push(true); // fill sign
		}
		uint8_t remain = len2 - fi.p2;
		if (remain < 2) {
			if (offset == 0) {
				// offset == 0 do not record
			}else {
				integerEncode_CBT(bw, offset, len2-col);
			}
			assert(fi.len == 1);
			// len == 1 do not record
		}else {
			integerEncode_CBT(bw, offset, len2-col);
			integerEncode_CBT(bw, fi.len-1, std::min(maxLen, remain));
		}

		if (col == 0) {
			col = fi.p2;
		}else {
			col += offset;
		}
		col += fi.len + 1;
	}
	if (col < len2s[row]) {
		bw.Push(false);
		cmds.push_back("next line");
	}
}

static
void buildHorizontalCommands(
	BitWriter& bw,
	std::vector<std::string>& cmds,
	const std::vector<FillInfo>& fills,
	uint8_t len1,
	uint8_t len2
	)
{
	if (fills.size() == 0) {
		// 全部改行！
		for (uint8_t i=0; i<len1; ++i) {
			cmds.push_back("row 0");
			bw.Push(false);
		}
		return;
	}
	
	// 空行かどうかの記録
	uint16_t lineFlags = 0;
	uint8_t maxLen = 1;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		maxLen = std::max(maxLen, fi.len);
		lineFlags |= 1 << fi.p1;
	}
	for (uint8_t i=0; i<len1; ++i) {
		bw.Push(lineFlags & (1<<i));
		if (lineFlags & (1<<i)) {
			cmds.push_back("row 1");
		}else {
			cmds.push_back("row 0");
		}
	}
	// 最大線長の記録
	assert(maxLen >= 2);
	char buff[32];
	sprintf(buff, "max line length : %d", maxLen);
	cmds.push_back(buff);
	integerEncode_CBT(bw, maxLen-2, len2-1);
	
	uint8_t col = 0;
	uint8_t row = fills[0].p1;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		if (row != fi.p1) {
			if (col < len2-1) {
				bw.Push(false);
				cmds.push_back("next line");
			}
			col = 0;
		}
		row = fi.p1;
		uint8_t offset = fi.p2 - col;
		
		char buff[32];
		sprintf(buff, "fill %d %d", offset, fi.len);
		cmds.push_back(buff);
		
		if (col != 0) {
			bw.Push(true); // fill sign
		}
		assert(fi.p2 <= len2-2);
		uint8_t remain = len2 - fi.p2;
		if (remain <= 2) {
			if (offset == 0) {
				// offset == 0 do not record
			}else {
				integerEncode_CBT(bw, offset, len2-1-col);
			}
			assert(fi.len == 2);
			// len == 1 do not record
		}else {
			integerEncode_CBT(bw, offset, len2-1-col);
			integerEncode_CBT(bw, fi.len-2, std::min(maxLen, remain)-1);
		}
		
		if (col == 0) {
			col = fi.p2;
		}else {
			col += offset;
		}
		col += fi.len + 1;
	}
	if (col < len2) {
		bw.Push(false);
		cmds.push_back("next line");
	}
}

bool isOverlappingWithFill(const std::vector<FillInfo>& fills, uint8_t p1, uint8_t p2)
{
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& vfi = fills[i];
		if (vfi.p1 != p1) {
			continue;
		}
		if (vfi.p2 <= p2 && p2 < vfi.p2+vfi.len) {
			return true;
		}
	}
	return false;
}

#define PIXEL_X 2
#define PIXEL_Y 4

bool isLineFullfilled(const std::vector<FillInfo>& fills, uint8_t p1, uint8_t w)
{
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		if (fi.p1 == p1 && fi.len == w) {
			return true;
		}
	}
	return false;
}

uint8_t findXRepeatLength(const Array2D<uint8_t>& values, uint8_t x, uint8_t y)
{
	if (!values[y][x]) {
		return 0;
	}
	uint8_t len = 1;
	for (int i=x-1; i!=-1; --i) {
		if (!values[y][i]) {
			break;
		}
		++len;
	}
	for (int i=x+1; i<values.GetWidth(); ++i) {
		if (!values[y][i]) {
			break;
		}
		++len;
	}
}

uint8_t findYRepeatLength(const Array2D<uint8_t>& values, uint8_t x, uint8_t y)
{
	if (!values[y][x]) {
		return 0;
	}
	uint8_t len = 1;
	for (int i=y-1; i!=-1; --i) {
		if (!values[i][x]) {
			break;
		}
		++len;
	}
	for (int i=y+1; i<values.GetHeight(); ++i) {
		if (!values[i][x]) {
			break;
		}
		++len;
	}
	return len;
}

void searchFills(
	Array2D<uint8_t>& values,
	std::vector<FillInfo>& hFills,
	std::vector<FillInfo>& vFills,
	uint8_t* vlens
	)
{
	hFills.clear();
	vFills.clear();
	
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	
	// 横方向の線の完全塗りつぶしを最初に調査
	if (w > 1) {
		for (uint8_t y=0; y<h; ++y) {
			uint8_t x;
			for (x=0; x<w; ++x) {
				if (!values[y][x]) {
					break;
				}
			}
			if (x != w) {
				continue;
			}
			FillInfo fi;
			fi.p1 = y;
			fi.p2 = 0;
			fi.len = w;
			hFills.push_back(fi);

			for (x=0; x<w; ++x) {
				values[y][x] = PIXEL_X;
			}
		}
	}
	// 横方向の塗りつぶし情報収集
	for (uint8_t y=0; y<h; ++y) {
		// 完全に塗りつぶしされてる行は飛ばす
		if (isLineFullfilled(hFills, y, w)) {
			continue;
		}
		
		uint8_t* line = values[y];
		for (uint8_t x=0; x<w; ++x) {
			if (!line[x]) {
				continue;
			}
			// ドットがあった場合
			uint8_t ex;
			// 横方向の連続調査
			for (ex=x+1; ex<w; ++ex) {
				if (!line[ex]) {
					break;
				}
			}
			if (ex-x > 1) {
				// 横の連続塗りつぶし

				// 始点が長い縦線に含まれている場合は長さを減少
				if (findYRepeatLength(values, x, y) > ex-x) {
					++x;
				}
				// 終点が長い縦線に含まれている場合は長さを減少
				if (findYRepeatLength(values, ex-1, y) > ex-x) {
					// ただし終端の１つ手前まで来ている場合は改行情報を節約できるので減少しない。
					if (ex != w-1) {
						--ex;
					}
				}
				if (ex-x > 1) {
					FillInfo fi;
					fi.p1 = y;
					fi.p2 = x;
					fi.len = ex - x;
					hFills.push_back(fi);
					for (uint8_t i=x; i<ex; ++i) {
						line[i] = PIXEL_X;
					}
					x = ex;
					continue;
				}
			}

			//// 横方向に1pixel ////
			continue;
		}
	}

#if 1
	// 横方向の塗りつぶしの最適化
	uint8_t maxLen = 0;
	for (size_t i=0; i<hFills.size(); ++i) {
		maxLen = std::max(maxLen, hFills[i].len);
	}
	uint8_t lp1 = -1;
	for (size_t i=0; i<hFills.size(); ++i) {
		FillInfo& fi = hFills[i];
		// とりあえず行中の最初の塗りつぶしだけ対象にする。
		if (fi.p1 == lp1) {
			continue;
		}
		lp1 = fi.p1;
		
		if (fi.p2 > 0) {
			// 始点の左側に縦方向の塗りつぶしがある場合、左方向に延長しても容量が増えないかどうか判定
			if (values[fi.p1][fi.p2-1]) {
				assert(values[fi.p1][fi.p2-1] == 1);
				uint8_t oldBitLen = calcIntegerEncodedLength_CBT(fi.p2, w-1) + calcIntegerEncodedLength_CBT(fi.len-1, std::min(maxLen, (uint8_t)(w-fi.p2)));
				uint8_t newP2 = fi.p2 - 1;
				uint8_t newLen = fi.len + 1;
				if (newLen <= maxLen) {
					uint8_t newBitLen = calcIntegerEncodedLength_CBT(newP2, w) + calcIntegerEncodedLength_CBT(newLen-1, std::min(maxLen, (uint8_t)(w-newP2)));
					if (newBitLen <= oldBitLen) {
						values[fi.p1][fi.p2-1] = PIXEL_X;
						--fi.p2;
						++fi.len;
					}
				}
			}
		}
		if (fi.p2+fi.len < w) {
			// 終点の右側に縦方向の塗りつぶしがある場合、右方向に延長しても容量が増えないかどうか判定
			if (values[fi.p1][fi.p2+fi.len]) {
				assert(values[fi.p1][fi.p2+fi.len] == 1);
				uint8_t oldBitLen = calcIntegerEncodedLength_CBT(fi.p2, w-1) + calcIntegerEncodedLength_CBT(fi.len-2, std::min(maxLen, (uint8_t)(w-fi.p2))-1);
				uint8_t newLen = fi.len + 1;
				if (newLen <= maxLen) {
					uint8_t newBitLen = calcIntegerEncodedLength_CBT(fi.p2, w-1) + calcIntegerEncodedLength_CBT(newLen-2, std::min(maxLen, (uint8_t)(w-fi.p2))-1);
					if (newBitLen <= oldBitLen) {
						assert(newBitLen == oldBitLen);
						values[fi.p1][fi.p2+fi.len] = PIXEL_X;
						++fi.len;
					}
				}
			}
		}
	}
#endif
	
	// 縦方向の塗りつぶし
	for (uint8_t x=0; x<w; ++x) {
		uint8_t cnt = 0;
		bool yBuff[32] = {false};
		// X記録を取り除いた縦線情報を収集
		for (uint8_t y=0; y<h; ++y) {
			uint8_t v = values[y][x];
			if (v != PIXEL_X) {
				if (v) {
					yBuff[cnt] = true;
					values[y][x] = PIXEL_Y;
				}
				++cnt;
			}
		}
		vlens[x] = cnt;
		// 縦線連続の情報を記録
		for (uint8_t y=0; y<cnt; ++y) {
			if (yBuff[y]) {
				uint8_t ey;
				for (ey=y+1; ey<cnt; ++ey) {
					if (!yBuff[ey]) {
						break;
					}
				}
				FillInfo fi;
				fi.p1 = x;
				fi.p2 = y;
				fi.len = ey - y;
				vFills.push_back(fi);
				y = ey;
			}
		}
	}
	
}

void push16(BitWriter& bw, uint16_t u)
{
	for (uint8_t i=0; i<16; ++i) {
		bw.Push(u & (1<<i));
	}
}

std::string EncodeHeader(
	BitWriter& bw,const BmpFontHeader& header
	)
{
//	testIntergerCoding();
	
	push16(bw, header.characterCount);
	assert(header.characterCount != 0);
	uint16_t code = header.characterCodes[0];
	push16(bw, code);
	uint16_t prevCode = code;

	uint16_t dist[4096] = {0};
	for (uint16_t i=1; i<header.characterCount; ++i) {
		uint16_t code = header.characterCodes[i];
		int diff = code - prevCode;
		assert(diff > 0);
		++dist[diff];
		integerEncode_Delta(bw, diff - 1);
		prevCode = code;
	}
	
	integerEncode_CBT(bw, header.minX, 16);
	integerEncode_CBT(bw, header.minY, 16);
	integerEncode_CBT(bw, header.maxW-1, 16);
	integerEncode_CBT(bw, header.maxH-1, 16);

	char buff[64];
	sprintf(buff, "%d %d %d %d\r\n", header.minX, header.minY, header.maxW, header.maxH);
	return buff;
}

std::string Encode(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo
	)
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	
	Array2D<uint8_t> values = bf.values_;
	uint8_t vlens[16];
	searchFills(values, hFills, vFills, vlens);
	std::sort(hFills.begin(), hFills.end());
	std::sort(vFills.begin(), vFills.end());
	
	std::vector<std::string> cmds;
	char buff[32];
	sprintf(buff, "(%d %d %d %d)", bf.x_, bf.y_, bf.w_, bf.h_);
	cmds.push_back(buff);
	
	uint8_t recX = bf.x_ - fontInfo.minX;
	uint8_t recY = bf.y_ - fontInfo.minY;
	uint8_t recW = fontInfo.maxW - (recX + bf.w_);
	uint8_t recH = fontInfo.maxH - (recY + bf.h_);
	// TODO: 0より1が圧倒的に多いので…。。ただ要適切に対処
	recX = (recX == 0 ? 15 : recX-1);
	integerEncode_Alpha(bw, recX);
	integerEncode_Alpha(bw, recY);
//	assert(bf.w_ != 0 && bf.h_ != 0);
	integerEncode_Alpha(bw, recW);
	integerEncode_Alpha(bw, recH);
	
	// dist
	{
		extern uint16_t g_dist[17][16];
		++g_dist[0][recX];
		++g_dist[1][recY];
		++g_dist[2][recW];
		++g_dist[3][recH];
	}

	BitWriter bw2;
	uint8_t buff2[128] = {0};
	bw2.Set(buff2);

	buildHorizontalCommands(bw2, cmds, hFills, bf.h_, bf.w_);
	buildVerticalCommands(bw2, cmds, vFills, bf.w_, vlens);
	
	if (bw2.GetNBits() < bf.w_*bf.h_) {
		bw.Push(true);
		buildHorizontalCommands(bw, cmds, hFills, bf.h_, bf.w_);
		buildVerticalCommands(bw, cmds, vFills, bf.w_, vlens);
	}else {
		bw.Push(false);
		for (uint8_t y=0; y<bf.h_; ++y) {
			for (uint8_t x=0; x<bf.w_; ++x) {
				bw.Push(bf.values_[y][x]);
			}
		}
	}
	
	std::string ret;
	for (size_t i=0; i<cmds.size(); ++i) {
		ret += cmds[i];
		ret += "\r\n";
	}
	
	return ret;
}

