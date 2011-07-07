#ifndef INTEGER_CODING_H_INCLUDED__
#define INTEGER_CODING_H_INCLUDED__

class BitReader;
class BitWriter;

void integerEncode_Alpha(BitWriter& bw, uint16_t n);
uint16_t integerDecode_Alpha(BitReader& br);

void integerEncode_Gamma(BitWriter& bw, uint16_t v);
uint16_t integerDecode_Gamma(BitReader& br);

void integerEncode_Delta(BitWriter& bw, uint16_t v);
uint16_t integerDecode_Delta(BitReader& br);

void integerEncode_CBT(BitWriter& bw, uint8_t n, uint8_t m);
uint8_t integerDecode_CBT(BitReader& br, uint8_t maxNum);
uint8_t calcIntegerEncodedLength_CBT(uint8_t n, uint8_t m);

void integerEncode_Custom14(BitWriter& bw, uint8_t n);

void testIntergerCoding();

#endif // #ifndef INTEGER_CODING_H_INCLUDED__
