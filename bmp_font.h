#ifndef RASTERIZER_H_INCLUDED__
#define RASTERIZER_H_INCLUDED__

#include <string>
#include "array2d.h"

class BitmapFont
{
public:
	BitmapFont()
		:
		values_(buff_)
	{
	}

	void Init(uint8_t w, uint8_t h);
	void Next();
	
	std::string Dump() const;
	
	void FillPixel(uint8_t x, uint8_t y);
	void Compact();
	
	Array2D<uint8_t> values_;
	uint8_t buff_[32*32];
	uint8_t x_;
	uint8_t y_;
	uint8_t w_;
	uint8_t h_;
	
};

struct BmpFontHeader
{
	uint16_t characterCount;
	uint16_t* characterCodes;
	uint8_t minX;
	uint8_t minY;
	uint8_t maxW;
	uint8_t maxH;
};

#endif // #ifndef RASTERIZER_H_INCLUDED__
