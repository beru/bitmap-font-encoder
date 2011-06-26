
#include "bmp_font_encoder.h"

#include <algorithm>

void BitmapFontEncoder::Init(uint8_t w, uint8_t h)
{
	x_ = 0;
	y_ = 0;
	w_ = w;
	h_ = h;
	idx_ = 0;
	dir_ = Dir_Horizontal;
	values_.Resize(w, h);
}

void BitmapFontEncoder::Next()
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

void BitmapFontEncoder::Fill(uint8_t start, uint8_t len)
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

std::string BitmapFontEncoder::Dump() const
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

static
void buildCommands(
	std::vector<std::string>& cmds,
	std::vector<FillInfo>& fills,
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

	assert(len2 > 0);
	if (fills.size() == 0) {
		// 全部改行！
		for (uint8_t i=0; i<len1; ++i) {
			cmds.push_back("newLine");
		}
	}else {
		uint8_t x = 0;
		uint8_t y = 0;
		for (size_t i=0; i<fills.size(); ++i) {
			// 手前の改行
			const FillInfo& fi = fills[i];
			for (uint8_t i=y; i<fi.p1; ++i) {
				cmds.push_back("newLine");
				x = 0;
			}
			y = fi.p1;

			char buff[32];
			sprintf(buff, "fill %d %d", fi.p2 - x, fi.len);
			cmds.push_back(buff);
			if (x == 0) {
				x = fi.p2;
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
		}
	}
}

std::string BitmapFontEncoder::BreakPixels() const
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	
	size_t idx0 = 0;
	FillInfo fi;
	for (uint8_t y=0; y<h_; ++y) {
		for (uint8_t x=0; x<w_; ++x) {
			if (!values_[idx0][x]) {
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
			if (y != h_ - 1) {
				if (!bVMatched) {
					uint8_t ylen = 1;
					// find vertical repeat
					for (uint8_t y2=y+1; y2<h_; ++y2) {
						if (!values_[y2][x]) {
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
			if (x != w_ - 1) {
				// find horizontal repeat
				for (uint8_t x2=x+1; x2<w_; ++x2) {
					if (!values_[idx0][x2]) {
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
	sprintf(buff, "(%d %d %d %d)", x_, y_, w_, h_);
	cmds.push_back(buff);

	buildCommands(cmds, hFills, h_, w_);
	buildCommands(cmds, vFills, w_, h_);
	
	std::string ret;
	for (size_t i=0; i<cmds.size(); ++i) {
		ret += cmds[i];
		ret += "\r\n";
	}
	
	return ret;
}

void BitmapFontEncoder::FillPixel(uint8_t x, uint8_t y)
{
	values_[y][x] = true;
}

void BitmapFontEncoder::Compact()
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