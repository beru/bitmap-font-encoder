#ifndef BMP_FONT_DECODE_H__
#define BMP_FONT_DECODE_H__

#include "bmp_font.h"

bool DecodeHeader(class BitReader& br, BmpFontHeader& header);
void Decode(class BitReader& br, BitmapFont& bf, const BmpFontHeader& fontInfo);

#endif // #ifndef BMP_FONT_DECODE_H__
