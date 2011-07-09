#include "chc.h"

#include <algorithm>
#include <functional>

bool compareEntryByCnt(const CHC_Entry& left, const CHC_Entry& right)
{
	if (left.cnt != right.cnt) {
		return left.cnt < right.cnt;
	}else {
		return left.code < right.code;
	}
}

bool compareEntryByBitLen(const CHC_Entry& left, const CHC_Entry& right)
{
	if (left.bitLen != right.bitLen) {
		return left.bitLen < right.bitLen;
	}else {
		return left.code < right.code;
	}
}

bool compareEntryByCode(const CHC_Entry& left, const CHC_Entry& right)
{
	assert(left.code != right.code);
	return left.code < right.code;
}

struct TreeNode
{
	uint16_t idx0;
	uint16_t idx1;
	uint16_t cnt;
	bool hasParent;
};

std::pair<size_t,size_t> findMinIdx(
	const TreeNode* nodes, int nodeStartIdx, int nodeLen,
	const CHC_Entry* entries, int entryStartIdx, int entryLen
	)
{
	size_t minCnt = -1;
	size_t minIdx = -1;
	assert(nodeLen < 100);
	assert(entryLen < 100);
	for (int i=nodeStartIdx; i<nodeLen; ++i) {
		const TreeNode& nd = nodes[i];
		if (nd.cnt < minCnt) {
			minCnt = nd.cnt;
			minIdx = 100+i;
		}
	}
	for (int i=entryStartIdx; i<entryLen; ++i) {
		const CHC_Entry& et = entries[i];
		if (et.cnt < minCnt) {
			minCnt = et.cnt;
			minIdx = i;
		}
	}
	return std::make_pair(minIdx, minCnt);
}

void incBitLength(
	int idx,
	int level,
	const TreeNode* nodes,
	CHC_Entry* entries
	);

void setBitLength(
	const TreeNode& node,
	int level,
	const TreeNode* nodes,
	CHC_Entry* entries
	)
{
	incBitLength(node.idx0, level, nodes, entries);
	incBitLength(node.idx1, level, nodes, entries);
}

void incBitLength(
	int idx,
	int level,
	const TreeNode* nodes,
	CHC_Entry* entries
	)
{
	if (idx < 100) {
		entries[idx].bitLen = level + 1;
	}else {
		setBitLength(nodes[idx-100], level+1, nodes, entries);
	}
}

// http://en.wikipedia.org/wiki/Canonical_Huffman_code

void BuildCanonicalHuffmanCodes(CHC_Entry* entries, size_t entryLen)
{
	TreeNode nodes[16] = {0};
	uint8_t nodeLen = 0;
	
	std::sort(entries, entries+entryLen, compareEntryByCnt);			
	int entryReadStart = 0;
	int nodeReadStart = 0;
	do {
		std::pair<size_t, size_t> ret0 = findMinIdx(nodes, nodeReadStart, nodeLen, entries, entryReadStart, entryLen);
		if (ret0.first >= 100) {
			++nodeReadStart;
		}else {
			++entryReadStart;
		}
		std::pair<size_t, size_t> ret1 = findMinIdx(nodes, nodeReadStart, nodeLen, entries, entryReadStart, entryLen);
		if (ret1.first >= 100) {
			++nodeReadStart;
		}else {
			++entryReadStart;
		}
		if (ret1.first == -1 || ret1.second == -1) {
			break;
		}
		TreeNode& nd = nodes[nodeLen++];
		nd.idx0 = ret0.first;
		nd.idx1 = ret1.first;
		nd.cnt = ret0.second + ret1.second;
	}while (1);
	const TreeNode& topNode = nodes[nodeLen-1];
	setBitLength(topNode, 0, nodes, entries);
	std::sort(entries, entries+entryLen, compareEntryByBitLen);
	
	uint16_t lastCHC = 0;
	uint8_t lastBitLen = entries[0].bitLen;
	uint8_t cnt = lastBitLen;
	for (size_t i=1; i<entryLen; ++i) {
		CHC_Entry& entry = entries[i];
		if (entry.bitLen == lastBitLen) {
			entry.chc = lastCHC + 1;
		}else {
			entry.chc = (lastCHC+1) << 1;
		}
		lastCHC = entry.chc;
		lastBitLen = entry.bitLen;
	}
	std::sort(entries, entries+entryLen, compareEntryByCode);
}


