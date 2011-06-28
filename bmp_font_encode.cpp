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

void encodeNum(BitWriter& bw, uint8_t num, uint8_t nBits)
{
	assert(nBits >= 1 && nBits <= 8);
	for (uint8_t i=0; i<nBits; ++i) {
		bw.Push( (num >> i) & 1 );
	}
}

static
void buildCommands(
	BitWriter& bw,
	std::vector<std::string>& cmds,
	const std::vector<FillInfo>& fills,
	uint8_t len1,
	uint8_t len2
	)
{
	uint8_t maxLen = 0;
	for (size_t i=0; i<fills.size(); ++i) {
		maxLen = std::max(maxLen, fills[i].len);
	}
	char buff[32];
	sprintf(buff, "maxLen(%d)", maxLen);
	cmds.push_back(buff);

	const uint8_t posBitLen = calcNumBits(len2);
	const uint8_t lenBitLen = std::max((uint8_t)1, calcNumBits(maxLen));
	encodeNum(bw, lenBitLen-1, 2);
	
	assert(len2 > 0);
	if (fills.size() == 0) {
		// 全部改行！
		for (uint8_t i=0; i<len1; ++i) {
			cmds.push_back("newLine");
			bw.Push(false);
		}
	}else {
		uint8_t x = 0;
		uint8_t y = 0;
		for (size_t i=0; i<fills.size(); ++i) {
			// 手前の改行
			const FillInfo& fi = fills[i];
			for (uint8_t i=y; i<fi.p1; ++i) {
				cmds.push_back("newLine");
				bw.Push(0);
				x = 0;
			}
			y = fi.p1;

			char buff[32];
			uint8_t offset = fi.p2 - x;
			sprintf(buff, "fill %d %d", offset, fi.len);
			cmds.push_back(buff);
			
			bw.Push(true);
			uint8_t diff = len2 - fi.p2;
			uint8_t lenBits = std::min(lenBitLen, calcNumBits(diff));
			if (lenBits == 0) {
				if (offset == 0) {
					// offset == 0 do not record
				}else {
					if (x == 0) {
						encodeNum(bw, offset, posBitLen);
					}else {
						encodeNum(bw, offset, std::min(posBitLen, calcNumBits(len2 - x)));
					}
				}
				assert(fi.len == 1);
				// len == 1 do not record
			}else {
				if (x == 0) {
					encodeNum(bw, fi.p2, posBitLen);
				}else {
					encodeNum(bw, offset, std::min(posBitLen, calcNumBits(len2-x)));
				}
				encodeNum(bw, fi.len-1, lenBits);
			}

			if (x == 0) {
				x = fi.p2;
			}else {
				x += offset;
			}
			x += fi.len + 1;

			if (fi.p2 + fi.len >= len2-1) {
				++y;
				x = 0;
			}
		}
		// 後続の改行
		for (uint8_t i=y; i<len1; ++i) {
			cmds.push_back("newLine");
			bw.Push(0);
		}
	}
}

void searchFills(
	const BitmapFont& bf,
	std::vector<FillInfo>& hFills,
	std::vector<FillInfo>& vFills
	)
{
	size_t idx0 = 0;
	FillInfo fi;
	for (uint8_t y=0; y<bf.h_; ++y) {
		for (uint8_t x=0; x<bf.w_; ++x) {
			if (!bf.values_[idx0][x]) {
				continue;
			}
			fi.p1 = y;
			fi.p2 = x;
			
			bool bVMatched = false;
			// find existing vFill entries
			for (size_t i=0; i<vFills.size(); ++i) {
				const FillInfo& vfi = vFills[i];
				if (vfi.p1 != x) {
					continue;
				}
				if (vfi.p2 <= y && y < vfi.p2+vfi.len) {
					bVMatched = true;
				}
			}
			// if not last row
			if (y != bf.h_ - 1) {
				if (!bVMatched) {
					uint8_t ylen = 1;
					// find vertical repeat
					for (uint8_t y2=y+1; y2<bf.h_; ++y2) {
						if (!bf.values_[y2][x]) {
							break;
						}
						++ylen;
					}
					if (ylen != 1) {
						FillInfo fi2;
						fi2.p1 = x;
						fi2.p2 = y;
						fi2.len = ylen;
						vFills.push_back(fi2);
						bVMatched = true;
					}
				}
			}

			uint8_t len = 1;
			// if last column
			if (x != bf.w_ - 1) {
				// find horizontal repeat
				for (uint8_t x2=x+1; x2<bf.w_; ++x2) {
					if (!bf.values_[idx0][x2]) {
						break;
					}
					++len;
				}
			}
			if (len == 1) {
				if (bVMatched) {
					continue;
				}
#if 1
				// 開始場所が出来るだけ後ろに位置する面に記録する方が、長さのビット数を短く出来る。
				if (bf.h_ - y < bf.w_ - x) {
					fi.p1 = x;
					fi.p2 = y;
					fi.len = 1;
					vFills.push_back(fi);
					continue;
				}
#endif
			}
			fi.len = len;
			hFills.push_back(fi);
			x += len - 1;
		}
		++idx0;
	}
}

std::string Encode(const BitmapFont& bf, BitWriter& bw)
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	
	searchFills(bf, hFills, vFills);
	std::sort(vFills.begin(), vFills.end());
	
	std::vector<std::string> cmds;
	char buff[32];
	sprintf(buff, "(%d %d %d %d)", bf.x_, bf.y_, bf.w_, bf.h_);
	cmds.push_back(buff);
	encodeNum(bw, bf.x_, 4);
	encodeNum(bw, bf.y_, 4);
	assert(bf.w_ != 0 && bf.h_ != 0);
	encodeNum(bw, bf.w_-1, 4);
	encodeNum(bw, bf.h_-1, 4);
	
	buildCommands(bw, cmds, hFills, bf.h_, bf.w_);
	buildCommands(bw, cmds, vFills, bf.w_, bf.h_);
	
	std::string ret;
	for (size_t i=0; i<cmds.size(); ++i) {
		ret += cmds[i];
		ret += "\r\n";
	}
	
	return ret;
}

