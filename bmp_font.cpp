#include "bmp_font.h"

void BitmapFont::Init(uint8_t w, uint8_t h)
{
	x_ = 0;
	y_ = 0;
	w_ = w;
	h_ = h;
	values_.Resize(w, h);
}

std::string BitmapFont::Dump() const
{
	std::string ret;
	for (uint8_t y=0; y<h_; ++y) {
		for (uint8_t x=0; x<w_; ++x) {
			ret.append(values_[y][x] ? "■" : "　");
		}
		ret.push_back('\r');
		ret.push_back('\n');
	}
	return ret;
}


void BitmapFont::FillPixel(uint8_t x, uint8_t y)
{
	values_[y][x] = 1;
}

void BitmapFont::Compact()
{
	uint8_t xMin = -1;
	uint8_t xMax = 0;
	for (uint8_t y=0; y<h_; ++y) {
		for (uint8_t x=0; x<w_; ++x) {
			if (values_[y][x]) {
				xMin = std::min(xMin, x);
				xMax = std::max(xMax, x);
			}
		}
	}

	uint8_t yMin = -1;
	uint8_t yMax = 0;
	for (uint8_t y=0; y<h_; ++y) {
		for (uint8_t x=0; x<w_; ++x) {
			if (values_[y][x]) {
				yMin = std::min(yMin, y);
				yMax = std::max(yMax, y);
			}
		}
	}
	if (xMin == (uint8_t)-1 || yMin == (uint8_t)-1) {
		x_ = 0;
		y_ = 0;
		w_ = 0;
		h_ = 0;
		values_.Resize(0,0);
		return;
	}

	uint8_t buff[32*32];
	Array2D<uint8_t> newValues(buff);
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

