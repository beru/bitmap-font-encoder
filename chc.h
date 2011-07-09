#ifndef CHC_H_INCLUDED__
#define CHC_H_INCLUDED__

struct CHC_Entry {
	uint16_t code;
	uint16_t cnt;
	uint16_t bitLen;
	uint16_t chc;
};

void BuildCanonicalHuffmanCodes(CHC_Entry* entries, size_t entryLen);

#endif // #ifndef CHC_H_INCLUDED__
