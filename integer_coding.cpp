#include "integer_coding.h"

#include "bit_reader.h"
#include "bit_writer.h"
#include "misc.h"

void integerEncode_Alpha(BitWriter& bw, uint16_t n)
{
	for (uint16_t i=0; i<n; ++i) {
		bw.Push(0);
	}
	bw.Push(1);
}

uint16_t integerDecode_Alpha(BitReader& br)
{
	uint16_t cnt = 0;
	while (br.Pop() == 0) {
		++cnt;
	}
	return cnt;
}

void integerEncode_Gamma(BitWriter& bw, uint16_t v)
{
	if (v == 0) {
		bw.Push(1);
		return;
	}
	uint8_t n = log2(v+1);
	uint16_t remain = v - (1<<n) + 1;
	integerEncode_Alpha(bw, n);
	for (uint8_t i=0; i<n; ++i) {
		bw.Push((remain >> (n-1-i)) & 1);
	}
}

uint16_t integerDecode_Gamma(BitReader& br)
{
	bool b = br.Pop();
	if (b) {
		return 0;
	}
	uint8_t n = 1 + integerDecode_Alpha(br);
	uint16_t remain = 0;
	for (uint8_t i=0; i<n; ++i) {
		remain |= br.Pop() << (n-1-i);
	}
	return (1 << n) - 1 + remain;
}

void integerEncode_Delta(BitWriter& bw, uint16_t v)
{
	if (v == 0) {
		bw.Push(1);
		return;
	}
	uint8_t n = log2(v+1);
	uint16_t remain = v - (1<<n) + 1;
	integerEncode_Gamma(bw, n);
	for (uint8_t i=0; i<n; ++i) {
		bw.Push((remain >> (n-1-i)) & 1);
	}
}

uint16_t integerDecode_Delta(BitReader& br)
{
	bool b = br.Front();
	if (b) {
		br.Pop();
		return 0;
	}
	uint8_t n = integerDecode_Gamma(br);
	uint16_t remain = 0;
	for (uint8_t i=0; i<n; ++i) {
		remain |= br.Pop() << (n-1-i);
	}
	return (1 << n) + remain - 1;
}

void integerEncode_CBT(BitWriter& bw, uint8_t n, uint8_t m)
{
	assert(m >= 1 && m <= 16);
	assert(n >= 0 && n < m);
	if (m == 1) {
		return;
	}
	uint8_t p2 = pow2roundup(m);
	if (n < p2-m) {
		uint8_t nBits = countBits8(p2-1)-1;
		for (uint8_t i=0; i<nBits; ++i) {
			bw.Push((n >> (nBits-1-i)) & 1);
		}
	}else {
		uint8_t nBits = countBits8(p2-1);
		n += p2 - m;
		for (uint8_t i=0; i<nBits; ++i) {
			bw.Push((n >> (nBits-1-i)) & 1);
		}
	}
}

uint8_t integerDecode_CBT(BitReader& br, uint8_t maxNum)
{
	assert(maxNum >= 1 && maxNum <= 16);
	if (maxNum == 1) {
		return 0;
	}
	uint8_t p2 = pow2roundup(maxNum);
	uint8_t nBits = countBits8(p2-1)-1;
	uint8_t ret = 0;
	for (uint8_t i=0; i<nBits; ++i) {
		ret |= br.Pop() << (nBits-1-i);
	}
	if (ret >= p2-maxNum) {
		ret <<= 1;
		ret |= br.Pop();
		ret -= p2-maxNum;
	}
	assert(ret >= 0 && ret <= maxNum);
	return ret;
}

uint8_t calcIntegerEncodedLength_CBT(uint8_t n, uint8_t m)
{
	assert(n >= 0 && n < m);
	if (m == 1) {
		return 0;
	}
	uint8_t p2 = pow2roundup(m);
	uint8_t nBits = countBits8(p2-1);
	if (n < p2-m) {
		return nBits - 1;
	}else {
		return nBits;
	}
}

void testIntergerCoding()
{
	uint8_t buff[512];
	BitWriter bw;
	bw.Set(buff);
	for (uint16_t i=0; i<255; ++i) {
		integerEncode_Delta(bw, i);
//TRACE("\r\n");
	}
	
	BitReader br;
	br.Set(buff);
	
	for (uint16_t i=0; i<255; ++i) {
		uint16_t val = integerDecode_Delta(br);
		TRACE("%d\r\n", val);
	}
	
}

