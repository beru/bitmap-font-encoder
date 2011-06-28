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

bool isOverlappingWithVFill(const std::vector<FillInfo>& vFills, uint8_t x, uint8_t y)
{
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& vfi = vFills[i];
		if (vfi.p1 != x) {
			continue;
		}
		if (vfi.p2 <= y && y < vfi.p2+vfi.len) {
			return true;
		}
	}
	return false;
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
			
			if (isOverlappingWithVFill(vFills, x, y)) {
				continue;
			}
			// if not last row
#if 1
			uint8_t vRepeatLen = 1;
			if (y != bf.h_ - 1) {
				// find vertical repeat
				for (uint8_t y2=y+1; y2<bf.h_; ++y2) {
					if (!bf.values_[y2][x]) {
						break;
					}
					++vRepeatLen;
				}
			}
#else
			if (y != bf.h_ - 1) {
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
					continue;
				}
			}
#endif

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
				// 線の末尾が縦方向の線と重なる場合は長さを短くする
				if (len > 1 && isOverlappingWithVFill(vFills, x+len-1, y)) {
					--len;
				}
			}
#if 1
			if (vRepeatLen != 1) {
				if (vRepeatLen > len) {
					FillInfo fi2;
					fi2.p1 = x;
					fi2.p2 = y;
					fi2.len = vRepeatLen;
					vFills.push_back(fi2);
					continue;
				}else {
					FillInfo fi2;
					fi2.p1 = x;
					fi2.p2 = y + 1;
					fi2.len = vRepeatLen - 1;
					vFills.push_back(fi2);
				}
			}
#endif
			if (len == 1) {
				// 開始場所が出来るだけ後ろに位置する面に記録する方が、長さのビット数を短く出来るので
				if (bf.h_ - y < bf.w_ - x) {
					fi.p1 = x;
					fi.p2 = y;
					fi.len = 1;
					vFills.push_back(fi);
					continue;
				}
			}
			fi.len = len;
			hFills.push_back(fi);
			x += len - 1;
		}
		++idx0;
	}
}

void optimizeFills(
	std::vector<FillInfo>& hFills,
	std::vector<FillInfo>& vFills
	)
{
	// 長い横線の端を縦線に移動して長さを出来るだけ短くする最適化
	uint8_t hMaxLen = 0;
	uint8_t vMaxLen = 0;
	for (size_t i=0; i<hFills.size(); ++i) {
		const FillInfo& fi = hFills[i];
		hMaxLen = std::max(hMaxLen, fi.len);
	}
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		vMaxLen = std::max(vMaxLen, fi.len);
	}
	uint8_t hMaxLenBitLen = calcNumBits(hMaxLen);
	uint8_t vMaxLenBitLen = calcNumBits(vMaxLen);
	for (size_t i=0; i<hFills.size(); ++i) {
		FillInfo& fi = hFills[i];
		if (fi.len == 1 || fi.len != hMaxLen) {
			continue;
		}
		do {
			bool bReduced = false;
			for (size_t j=0; j<vFills.size(); ++j) {
				FillInfo& vf = vFills[j];
				if (vf.p1 != fi.p2) {
					continue;
				}
				// 次の行に縦線がある場合
				if (vf.p2 == fi.p1+1) {
					uint8_t bitLen = calcNumBits(vf.len);
					uint8_t newBitLen = calcNumBits(vf.len+1);
					if (bitLen == newBitLen || newBitLen <= vMaxLenBitLen) {
						++fi.p2;
						--fi.len;
						--vf.p2;
						++vf.len;
						hMaxLen = fi.len;
						hMaxLenBitLen = calcNumBits(hMaxLen);
						bReduced = true;
					}
				}else if (vf.len != 1 || vf.p2+vf.len-1 == fi.p1) {
					++fi.p2;
					--fi.len;
					hMaxLen = fi.len;
					hMaxLenBitLen = calcNumBits(hMaxLen);
					bReduced = true;
				}
				if (fi.len == 1) {
					break;
				}
			}
			if (!bReduced) {
				break;
			}
		} while (fi.len > 1);
	}
}

std::string Encode(const BitmapFont& bf, BitWriter& bw)
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	
	searchFills(bf, hFills, vFills);
	optimizeFills(hFills, vFills);
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

