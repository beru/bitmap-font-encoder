#ifndef BMP_FONT_ENCODE_H__
#define BMP_FONT_ENCODE_H__

#include "bmp_font.h"

std::string EncodeHeader(class BitWriter& bw, uint8_t minX, uint8_t minY, uint8_t maxW, uint8_t maxH);
std::string Encode(class BitWriter& bw, const BitmapFont& bf, uint8_t minX, uint8_t minY, uint8_t maxW, uint8_t maxH);

#endif // #ifndef BMP_FONT_ENCODE_H__
