#include "bmp_font_encode.h"

#include <algorithm>
#include "bit_writer.h"
#include "misc.h"

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

// http://www.hackersdelight.org/HDcode/flp2.c.txt
/* Round down to a power of 2. */
unsigned flp2_16(uint16_t x) {
   x = x | (x >> 1);
   x = x | (x >> 2);
   x = x | (x >> 4);
   x = x | (x >> 8);
   return x - (x >> 1);
}

uint8_t log2(uint32_t v) {
	int r;      // result goes here

	static const int MultiplyDeBruijnBitPosition[32] = 
	{
	  0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
	  8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
	};

	v |= v >> 1; // first round down to one less than a power of 2 
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;

	r = MultiplyDeBruijnBitPosition[(uint32_t)(v * 0x07C4ACDDU) >> 27];
	return r;
}

void integerEncode_Alpha(BitWriter& bw, uint16_t n)
{
	for (uint16_t i=0; i<n; ++i) {
		bw.Push(0);
	}
	bw.Push(1);
}

void integerEncode_Gamma(BitWriter& bw, uint16_t v)
{
	if (v == 0) {
		bw.Push(1);
		return;
	}
	uint8_t n = log2(v+1);
	uint16_t remain = v - (1<<n) + 1;
	integerEncode_Alpha(bw, n);
	for (uint8_t i=0; i<n; ++i) {
		bw.Push((remain >> (n-1-i)) & 1);
	}
}

void integerEncode_Delta(BitWriter& bw, uint16_t v)
{
	if (v == 0) {
		bw.Push(1);
		return;
	}
	uint8_t n = log2(v+1);
	uint16_t remain = v - (1<<n) + 1;
	integerEncode_Gamma(bw, n);
	for (uint8_t i=0; i<n; ++i) {
		bw.Push((remain >> (n-1-i)) & 1);
	}
}

void testIntergerCoding()
{
	uint8_t buff[128];
	BitWriter bw;
	bw.Set(buff);
	for (uint16_t i=0; i<255; ++i) {
		integerEncode_Gamma(bw, i);
TRACE("\r\n");
	}
}

void encodeNum(BitWriter& bw, uint8_t n, uint8_t m)
{
	assert(m >= 1 && m <= 16);
	assert(n >= 0 && n < m);
	if (m == 1) {
		return;
	}
	uint8_t p2 = pow2roundup(m);
	if (n < p2-m) {
		uint8_t nBits = countBits8(p2-1)-1;
		for (uint8_t i=0; i<nBits; ++i) {
			bw.Push((n >> (nBits-1-i)) & 1);
		}
	}else {
		uint8_t nBits = countBits8(p2-1);
		n += p2 - m;
		for (uint8_t i=0; i<nBits; ++i) {
			bw.Push((n >> (nBits-1-i)) & 1);
		}
	}
}

uint8_t calcEncodedLen(uint8_t n, uint8_t m)
{
	assert(n >= 0 && n < m);
	if (m == 1) {
		return 0;
	}
	uint8_t p2 = pow2roundup(m);
	uint8_t nBits = countBits8(p2-1);
	if (n < p2-m) {
		return nBits - 1;
	}else {
		return nBits;
	}
}

static
void buildCommands(
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
	for (uint8_t i=0; i<len1; ++i) {
		maxLen2 = std::max(maxLen2, len2s[i]);
	}
	encodeNum(bw, maxLen-1, maxLen2);
	
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
				encodeNum(bw, offset, len2-col);
			}
			assert(fi.len == 1);
			// len == 1 do not record
		}else {
			encodeNum(bw, offset, len2-col);
			encodeNum(bw, fi.len-1, std::min(maxLen, remain));
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

			// 上下に縦方向の連続塗りつぶしがある場合
			if (
				(y != 0 && values[y-1][x])
				|| (y != h-1 && values[y+1][x])
			) {
				// もし最後の列で１つ前の位置に横の記録があるなら、改行の1bitも、データがあるの1bitも容量は同じなのでデータを置く。
				if (x == w-1 && values[y][x-2] == PIXEL_X) {

				}else {
					continue;
				}
			}
			
			// 横方向の最後に置いてある単独の塗りつぶしは改行情報も長さ情報も省けるので、横方向で採用する。
			if (x != w-1) {
				// 縦方向の最後や最後の１つ手前にある単独の塗りつぶしは、縦方向で採用すればそちらの改行情報を省けるので、横方向では採用しない。
				if (y >= h-2) {
					continue;
				}
				// 幅１の塗りつぶしで、左端からの距離より上端からの距離の方が大きい場合は、横方向では採用しない。
				if (w-x > h-y) {
					continue;
				}
			}

			FillInfo fi;
			fi.p1 = y;
			fi.p2 = x;
			fi.len = 1;
			hFills.push_back(fi);
			line[x] = PIXEL_X;
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
				uint8_t oldBitLen = calcEncodedLen(fi.p2, w) + calcEncodedLen(fi.len-1, std::min(maxLen, (uint8_t)(w-fi.p2)));
				uint8_t newP2 = fi.p2 - 1;
				uint8_t newLen = fi.len + 1;
				if (newLen <= maxLen) {
					uint8_t newBitLen = calcEncodedLen(newP2, w) + calcEncodedLen(newLen-1, std::min(maxLen, (uint8_t)(w-newP2)));
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
				uint8_t oldBitLen = calcEncodedLen(fi.p2, w) + calcEncodedLen(fi.len-1, std::min(maxLen, (uint8_t)(w-fi.p2)));
				uint8_t newLen = fi.len + 1;
				if (newLen <= maxLen) {
					uint8_t newBitLen = calcEncodedLen(fi.p2, w) + calcEncodedLen(newLen-1, std::min(maxLen, (uint8_t)(w-fi.p2)));
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
	BitWriter& bw,
	uint16_t strCount, const uint16_t* codes,
	uint8_t minX, uint8_t minY, uint8_t maxW, uint8_t maxH
	)
{
//	testIntergerCoding();
	
	push16(bw, strCount);
	assert(strCount != 0);
	uint16_t code = codes[0];
	push16(bw, code);
	uint16_t prevCode = code;

	uint16_t dist[4096] = {0};
	for (uint16_t i=1; i<strCount; ++i) {
		uint16_t code = codes[i];
		int diff = code - prevCode;
		assert(diff > 0);
		++dist[diff];
		integerEncode_Delta(bw, diff - 1);
		prevCode = code;
	}
	
	encodeNum(bw, minX, 16);
	encodeNum(bw, minY, 16);
	encodeNum(bw, maxW-1, 16);
	encodeNum(bw, maxH-1, 16);

	char buff[64];
	sprintf(buff, "%d %d %d %d\r\n", minX, minY, maxW, maxH);
	return buff;
}

std::string Encode(
	BitWriter& bw,
	const BitmapFont& bf,
	uint8_t minX, uint8_t minY, uint8_t maxW, uint8_t maxH
	
	)
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	
	Array2D<uint8_t> values = bf.values_;
	uint8_t hlens[16];
	for (uint8_t i=0; i<bf.h_; ++i) { hlens[i] = bf.w_; }
	uint8_t vlens[16];
	searchFills(values, hFills, vFills, vlens);
	std::sort(hFills.begin(), hFills.end());
	std::sort(vFills.begin(), vFills.end());
	
	std::vector<std::string> cmds;
	char buff[32];
	sprintf(buff, "(%d %d %d %d)", bf.x_, bf.y_, bf.w_, bf.h_);
	cmds.push_back(buff);
	
	uint8_t recX = bf.x_ - minX;
	uint8_t recY = bf.y_ - minY;
	// TODO: 0より1が圧倒的に多いので…。。ただ要適切に対処
	integerEncode_Alpha(bw, (recX == 0 ? 15 : recX-1));
	integerEncode_Alpha(bw, recY);
//	assert(bf.w_ != 0 && bf.h_ != 0);
	integerEncode_Alpha(bw, maxW - (recX + bf.w_));
	integerEncode_Alpha(bw, maxH - (recY + bf.h_));
	
	// dist
	{
		extern uint16_t g_dist[17][16];
		++g_dist[0][bf.x_];
		++g_dist[1][bf.y_];
		++g_dist[2][bf.w_];
		++g_dist[3][bf.w_];
	}
	
	buildCommands(bw, cmds, hFills, bf.h_, hlens);
	buildCommands(bw, cmds, vFills, bf.w_, vlens);
	
	std::string ret;
	for (size_t i=0; i<cmds.size(); ++i) {
		ret += cmds[i];
		ret += "\r\n";
	}
	
	return ret;
}

