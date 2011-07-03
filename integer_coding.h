#ifndef INTEGER_CODING_H_INCLUDED__
#define INTEGER_CODING_H_INCLUDED__

class BitReader;
class BitWriter;

void integerEncode_Alpha(BitWriter& bw, uint16_t n);
void integerEncode_Gamma(BitWriter& bw, uint16_t v);
void integerEncode_Delta(BitWriter& bw, uint16_t v);
void integerEncode_CBT(BitWriter& bw, uint8_t n, uint8_t m);
uint8_t calcIntegerEncodedLength_CBT(uint8_t n, uint8_t m);

uint16_t integerDecode_Alpha(BitReader& br);
uint16_t integerDecode_Gamma(BitReader& br);
uint16_t integerDecode_Delta(BitReader& br);
uint8_t integerDecode_CBT(BitReader& br, uint8_t maxNum);

void testIntergerCoding();

#endif // #ifndef INTEGER_CODING_H_INCLUDED__
