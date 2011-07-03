#ifndef BMP_FONT_ENCODE_H__
#define BMP_FONT_ENCODE_H__

#include "bmp_font.h"

std::string EncodeHeader(class BitWriter& bw, const BmpFontHeader& header);
std::string Encode(class BitWriter& bw, const BitmapFont& bf, const BmpFontHeader& header);

#endif // #ifndef BMP_FONT_ENCODE_H__
