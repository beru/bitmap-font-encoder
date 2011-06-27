
#include "bmp_font.h"

#include <algorithm>
#include <cmath>
#include "bit_writer.h"
#include "bit_reader.h"

void BitmapFont::Init(uint8_t w, uint8_t h)
{
	x_ = 0;
	y_ = 0;
	w_ = w;
	h_ = h;
	idx_ = 0;
	dir_ = Dir_Horizontal;
	values_.Resize(w, h);
}

void BitmapFont::Next()
{
	++idx_;
	if (dir_ == Dir_Horizontal) {
		if (idx_ == h_) {
			dir_ = Dir_Vertical;
			idx_ = 0;
		}
	}else {
		if (idx_ == w_) {
			dir_ = Dir_Horizontal;
			idx_ = 0;
		}
	}
}

void BitmapFont::Fill(uint8_t start, uint8_t len)
{
	if (dir_ == Dir_Horizontal) {
		for (uint8_t i=0; i<len; ++i) {
			values_[idx_][i] = true;
		}
		if (start + len >= w_) {
			Next();
		}
	}else {
		size_t startIdx = idx_ + start * w_;
		for (uint8_t i=0; i<len; ++i) {
			values_[start+i][idx_] = true;
		}
		if (start + len >= h_) {
			Next();
		}
	}
}

std::string BitmapFont::Dump() const
{
	std::string ret;
	for (uint8_t y=0; y<h_; ++y) {
		for (uint8_t x=0; x<w_; ++x) {
			ret.push_back(values_[y][x] ? '#' : '-');
		}
		ret.push_back('\r');
		ret.push_back('\n');
	}
	return ret;
}

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

uint8_t decodeNum(BitReader& br, uint8_t nBits)
{
	assert(nBits >= 1 && nBits <= 8);
	uint8_t ret = 0;
	for (uint8_t i=0; i<nBits; ++i) {
		if (br.Pop()) {
			ret += (1 << i);
		}
	}
	return ret;
}

uint8_t calcNumBits(uint8_t len)
{
	return (uint8_t) std::ceil(std::log((double)len) / std::log(2.0));
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

std::string Encode(const BitmapFont& bf, BitWriter& bw)
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	
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
			}
			fi.len = len;
			hFills.push_back(fi);
			x += len - 1;
		}
		++idx0;
	}
	
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

void decodeFills(
	BitReader& br,
	std::vector<FillInfo>& fills,
	uint8_t len1,	// 行方向の長さ
	uint8_t len2	// 列方向の長さ
	)
{
	const uint8_t posBitLen = calcNumBits(len2);
	uint8_t lenBitLen = decodeNum(br, 2) + 1;
	uint8_t row = 0;
	uint8_t col = 0;
	while (row < len1) {
		bool b = br.Pop();
		if (!b) {
			++row;
			col = 0;
			continue;
		}
		uint8_t offset;
		uint8_t fillLen;
		if (col == 0) {
			offset = decodeNum(br, posBitLen);
			uint8_t maxLenBitLen = calcNumBits(len2-(col+offset));
			if (maxLenBitLen == 0) {
				fillLen = 1;
			}else {
				fillLen = decodeNum(br, std::min(lenBitLen, maxLenBitLen)) + 1;
			}
		}else {
			uint8_t diff = len2 - col;
			if (diff == 1) {
				offset = 0;
				fillLen = 1;
			}else {
				uint8_t nBits = std::min(posBitLen, calcNumBits(diff));
				offset = decodeNum(br, nBits);
				uint8_t maxLenBitLen = calcNumBits(len2-(col+offset));
				if (maxLenBitLen == 0) {
					fillLen = 1;
				}else {
					fillLen = decodeNum(br, std::min(lenBitLen, maxLenBitLen)) + 1;
				}
			}
		}
		FillInfo fi;
		fi.p1 = row;
		fi.p2 = col + offset;
		fi.len = fillLen;
		fills.push_back(fi);
		col += offset + fillLen + 1;
		if (col >= len2) {
			++row;
			col = 0;
		}
	}
}

void Decode(BitmapFont& bf, BitReader& br)
{
	uint8_t x = decodeNum(br, 4);
	uint8_t y = decodeNum(br, 4);
	uint8_t w = decodeNum(br, 4) + 1;
	uint8_t h = decodeNum(br, 4) + 1;
	bf.Init(w, h);
	bf.x_ = x;
	bf.y_ = y;
	
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	decodeFills(br, hFills, h, w);
	decodeFills(br, vFills, w, h);
	
	for (size_t i=0; i<hFills.size(); ++i) {
		const FillInfo& fi = hFills[i];
		for (uint8_t j=0; j<fi.len; ++j) {
			bf.FillPixel(fi.p2+j, fi.p1);
		}
	}
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		for (uint8_t j=0; j<fi.len; ++j) {
			bf.FillPixel(fi.p1, fi.p2+j);
		}
	}
}

void BitmapFont::FillPixel(uint8_t x, uint8_t y)
{
	values_[y][x] = true;
}

void BitmapFont::Compact()
{
	uint8_t xMin = w_ - 1;
	uint8_t xMax = 0;
	for (uint8_t y=0; y<h_; ++y) {
		for (uint8_t x=0; x<w_; ++x) {
			if (values_[y][x]) {
				xMin = std::min(xMin, x);
				xMax = std::max(xMax, x);
			}
		}
	}

	uint8_t yMin = h_ - 1;
	uint8_t yMax = 0;
	for (uint8_t y=0; y<h_; ++y) {
		for (uint8_t x=0; x<w_; ++x) {
			if (values_[y][x]) {
				yMin = std::min(yMin, y);
				yMax = std::max(yMax, y);
			}
		}
	}
	
	Array2D<bool> newValues;
	uint8_t nw = xMax + 1 - xMin;
	uint8_t nh = yMax + 1 - yMin;
	newValues.Resize(nw, nh);
	
	for (uint8_t y=0; y<nh; ++y) {
		for (uint8_t x=0; x<nw; ++x) {
			newValues[y][x] = values_[y+yMin][x+xMin];
		}
	}
	values_ = newValues;
	x_ = xMin;
	y_ = yMin;
	w_ = nw;
	h_ = nh;
	
}