#ifndef RASTERIZER_H_INCLUDED__
#define RASTERIZER_H_INCLUDED__

#include <string>
#include "array2d.h"

class BitmapFont
{
public:
	void Init(uint8_t w, uint8_t h);
	void Next();
	void Fill(uint8_t start, uint8_t len);
	
	std::string Dump() const;
	
	void FillPixel(uint8_t x, uint8_t y);
	void Compact();
	
	Array2D<bool> values_;
	uint8_t x_;
	uint8_t y_;
	uint8_t w_;
	uint8_t h_;

	enum Dir {
		Dir_Horizontal,
		Dir_Vertical,
	} dir_;
	uint8_t idx_;
};

void Decode(BitmapFont& bf, class BitReader& br);
std::string Encode(const BitmapFont& bf, class BitWriter& bw);


#endif // #ifndef RASTERIZER_H_INCLUDED__
