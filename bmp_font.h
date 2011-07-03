#ifndef RASTERIZER_H_INCLUDED__
#define RASTERIZER_H_INCLUDED__

#include <string>
#include "array2d.h"

class BitmapFont
{
public:
	void Init(uint8_t w, uint8_t h);
	void Next();
	
	std::string Dump() const;
	
	void FillPixel(uint8_t x, uint8_t y);
	void Compact();
	
	Array2D<uint8_t> values_;
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
