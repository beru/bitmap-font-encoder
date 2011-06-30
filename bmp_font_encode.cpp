#include "bmp_font_encode.h"

#include <algorithm>
#include "bit_writer.h"
#include "misc.h"

struct FillInfo
{
	uint8_t p1;
	uint8_t p2;
	uint8_t len;
};

bool operator < (const FillInfo& lhs, const FillInfo& rhs)
{
	return lhs.p1 < rhs.p1;
}

void encodeNum(BitWriter& bw, uint8_t n, uint8_t m)
{
	assert(m >= 1 && m <= 16);
	assert(n >= 0 && n < m);
	if (m == 1) {
		return;
	}
	uint8_t p2 = pow2roundup(m);
	if (n < p2-m) {
		uint8_t nBits = countBits(p2-1)-1;
		for (uint8_t i=0; i<nBits; ++i) {
			bw.Push((n >> (nBits-1-i)) & 1);
		}
	}else {
		uint8_t nBits = countBits(p2-1);
		n += p2 - m;
		for (uint8_t i=0; i<nBits; ++i) {
			bw.Push((n >> (nBits-1-i)) & 1);
		}
	}
}

#if 1

static
void buildCommands(
	BitWriter& bw,
	std::vector<std::string>& cmds,
	const std::vector<FillInfo>& fills,
	uint8_t len1,
	uint8_t len2
	)
{
	if (fills.size() == 0) {
		// �S�����s�I
		for (uint8_t i=0; i<len1; ++i) {
			cmds.push_back("row 0");
			bw.Push(false);
		}
		return;
	}
	
	// ��s���ǂ����̋L�^
	bool lineFlags[16] = {false};
	uint8_t maxLen = 1;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		maxLen = std::max(maxLen, fi.len);
		lineFlags[fi.p1] = true;
	}
	for (uint8_t i=0; i<len1; ++i) {
		bw.Push(lineFlags[i]);
		if (lineFlags[i]) {
			cmds.push_back("row 1");
		}else {
			cmds.push_back("row 0");
		}
	}
	// �ő�����̋L�^
	char buff[32];
	sprintf(buff, "max line length : %d", maxLen);
	cmds.push_back(buff);
	encodeNum(bw, maxLen-1, len2);
	
	uint8_t col = 0;
	uint8_t row = fills[0].p1;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		if (row != fi.p1) {
			if (col < len2) {
				bw.Push(false);
				cmds.push_back("next line");
			}
			col = 0;
		}
		row = fi.p1;
		char buff[32];
		uint8_t offset = fi.p2 - col;
		sprintf(buff, "fill %d %d", offset, fi.len);
		cmds.push_back(buff);
		
		if (col != 0) {
			bw.Push(true); // fill sign
		}
		uint8_t remain = len2 - fi.p2;
		if (remain < 2) {
			if (offset == 0) {
				// offset == 0 do not record
			}else {
				encodeNum(bw, offset, len2-col);
			}
			assert(fi.len == 1);
			// len == 1 do not record
		}else {
			encodeNum(bw, offset, len2-col);
			encodeNum(bw, fi.len-1, std::min(maxLen, remain));
		}

		if (col == 0) {
			col = fi.p2;
		}else {
			col += offset;
		}
		col += fi.len + 1;
	}
	if (col+1 < len2) {
		bw.Push(false);
		cmds.push_back("next line");
	}
}

#else

static
void buildCommands(
	BitWriter& bw,
	std::vector<std::string>& cmds,
	const std::vector<FillInfo>& fills,
	uint8_t len1,
	uint8_t len2
	)
{
	uint8_t maxLen = 1;
	for (size_t i=0; i<fills.size(); ++i) {
		maxLen = std::max(maxLen, fills[i].len);
	}
	char buff[32];
	sprintf(buff, "maxLen(%d)", maxLen);
	cmds.push_back(buff);
	
	encodeNum(bw, maxLen-1, len2);
	
	assert(len2 > 0);
	if (fills.size() == 0) {
		// �S�����s�I
		for (uint8_t i=0; i<len1; ++i) {
			cmds.push_back("newLine");
			bw.Push(false);
		}
	}else {
		
		uint8_t x = 0;
		uint8_t y = 0;
		for (size_t i=0; i<fills.size(); ++i) {
			// ��O�̉��s
			const FillInfo& fi = fills[i];
			for (uint8_t j=y; j<fi.p1; ++j) {
				cmds.push_back("newLine");
				bw.Push(0);
				x = 0;
			}
			y = fi.p1;

			char buff[32];
			uint8_t offset = fi.p2 - x;
			sprintf(buff, "fill %d %d", offset, fi.len);
			cmds.push_back(buff);
			
			bw.Push(true); // fill sign
			uint8_t remain = len2 - fi.p2;
			if (remain < 2) {
				if (offset == 0) {
					// offset == 0 do not record
				}else {
					encodeNum(bw, offset, len2-x);
				}
				assert(fi.len == 1);
				// len == 1 do not record
			}else {
				encodeNum(bw, offset, len2-x);
				encodeNum(bw, fi.len-1, std::min(maxLen, remain));
			}

			if (x == 0) {
				x = fi.p2;
			}else {
				x += offset;
			}
			x += fi.len + 1;

			// �Ō�̗�̂P��O�܂ŐL�΂�����A��͉��s���������B
			if (fi.p2 + fi.len + 1 >= len2) {
				++y;
				x = 0;
			}
		}
		// �㑱�̉��s
		for (uint8_t i=y; i<len1; ++i) {
			cmds.push_back("newLine");
			bw.Push(0);
		}
	}
}

#endif

bool isOverlappingWithFill(const std::vector<FillInfo>& fills, uint8_t p1, uint8_t p2)
{
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& vfi = fills[i];
		if (vfi.p1 != p1) {
			continue;
		}
		if (vfi.p2 <= p2 && p2 < vfi.p2+vfi.len) {
			return true;
		}
	}
	return false;
}

void searchFills(
	const BitmapFont& bf,
	std::vector<FillInfo>& hFills,
	std::vector<FillInfo>& vFills
	)
{
	size_t idx0 = 0;
	FillInfo fi;
	for (uint8_t y=0; y<bf.h_; ++y) {
		for (uint8_t x=0; x<bf.w_; ++x) {
			if (!bf.values_[idx0][x]) {
				continue;
			}
			fi.p1 = y;
			fi.p2 = x;
			
			if (isOverlappingWithFill(vFills, x, y)) {
				continue;
			}
			// if not last row
			uint8_t vRepeatLen = 1;
			if (y != bf.h_ - 1) {
				// find vertical repeat
				for (uint8_t y2=y+1; y2<bf.h_; ++y2) {
					if (!bf.values_[y2][x]) {
						break;
					}
					++vRepeatLen;
				}
			}
			uint8_t len = 1;
			// if last column
			if (x != bf.w_ - 1) {
				// find horizontal repeat
				for (uint8_t x2=x+1; x2<bf.w_; ++x2) {
					if (!bf.values_[idx0][x2]) {
						break;
					}
					++len;
				}
				// ���̖������c�����̐��Əd�Ȃ�ꍇ�͒�����Z������
				if (len > 1 && isOverlappingWithFill(vFills, x+len-1, y)) {
					--len;
				}
			}
			if (vRepeatLen != 1) {
				if (vRepeatLen > len) {
					FillInfo fi2;
					fi2.p1 = x;
					fi2.p2 = y;
					fi2.len = vRepeatLen;
					vFills.push_back(fi2);
					continue;
				}else {
					FillInfo fi2;
					fi2.p1 = x;
					fi2.p2 = y + 1;
					fi2.len = vRepeatLen - 1;
					vFills.push_back(fi2);
				}
			}
			if (len == 1) {
				// �J�n�ꏊ���o���邾�����Ɉʒu����ʂɋL�^��������A�����̃r�b�g����Z���o����̂�
				if (bf.h_ - y < bf.w_ - x) {
					fi.p1 = x;
					fi.p2 = y;
					fi.len = 1;
					vFills.push_back(fi);
					continue;
				}
			}
			fi.len = len;
			hFills.push_back(fi);
			x += len - 1;
		}
		++idx0;
	}
}

void optimizeFills(
	std::vector<FillInfo>& hFills,
	std::vector<FillInfo>& vFills
	)
{
	// ���������̒[���c���Ɉړ����Ē������o���邾���Z������œK��
	uint8_t hMaxLen = 0;
	uint8_t vMaxLen = 0;
	for (size_t i=0; i<hFills.size(); ++i) {
		const FillInfo& fi = hFills[i];
		hMaxLen = std::max(hMaxLen, fi.len);
	}
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		vMaxLen = std::max(vMaxLen, fi.len);
	}
	uint8_t hMaxLenBitLen = calcNumBits(hMaxLen);
	uint8_t vMaxLenBitLen = calcNumBits(vMaxLen);
	for (size_t i=0; i<hFills.size(); ++i) {
		FillInfo& fi = hFills[i];
		if (fi.len != hMaxLen) {
			continue;
		}
		while (fi.len > 1) {
			bool bReduced = false;
			for (size_t j=0; j<vFills.size(); ++j) {
				FillInfo& vf = vFills[j];
				// �n�_����
				if (fi.p2 == vf.p1) {
					// ���̍s�ɏc��������ꍇ
					if (vf.p2 == fi.p1+1) {
						uint8_t bitLen = calcNumBits(vf.len);
						uint8_t newBitLen = calcNumBits(vf.len+1);
						if (bitLen == newBitLen || newBitLen <= vMaxLenBitLen) {
							++fi.p2;
							--fi.len;
							--vf.p2;
							++vf.len;
							hMaxLen = fi.len;
							hMaxLenBitLen = calcNumBits(hMaxLen);
							bReduced = true;
						}
					}else if (vf.len != 1 && vf.p2+vf.len-1 == fi.p1) {
						// �c���̏I�_����������Ή����̎n�_���E�ɂ��点��
						++fi.p2;
						--fi.len;
						hMaxLen = fi.len;
						hMaxLenBitLen = calcNumBits(hMaxLen);
						bReduced = true;
					}
				}else if (fi.p2+fi.len-1 == vf.p1) {
					// �c���̎n�_����ɉ�������΁A�����̏I�_�����ɂ��点��
					if (vf.p2 != 0 && vf.p2-1 == fi.p1) {
						uint8_t newBitLen = calcNumBits(vf.len+1);
						if (newBitLen == vMaxLenBitLen) {
							--fi.len;
							hMaxLen = fi.len;
							hMaxLenBitLen = calcNumBits(hMaxLen);
							bReduced = true;
							--vf.p2;
							++vf.len;
							vMaxLen = std::max(vMaxLen, vf.len);
						}
					}
				}
				if (fi.len == 1) {
					break;
				}
			}
			if (!bReduced) {
				break;
			}
		}
	}
	if (vMaxLen == 1) {
		return;
	}
	// �����c���̒������o���邾���Z������œK���i���������̒����̍œK�������Ƌ��ʉ��o���邯��ǁc�j
	for (size_t i=0; i<vFills.size(); ++i) {
		FillInfo& fi = vFills[i];
		if (fi.len != vMaxLen) {
			continue;
		}
		// �n�_�������ƂԂ����Ă�����n�_�����炷�B
		if (isOverlappingWithFill(hFills, fi.p2, fi.p1)) {
			++fi.p2;
			--fi.len;
		}
		// �I�_�������ƂԂ����Ă�����I�_�����炷
		if (isOverlappingWithFill(hFills, fi.p2+fi.len-1, fi.p1)) {
			--fi.len;
		}
		for (size_t j=0; j<hFills.size(); ++j) {
			FillInfo& hf = hFills[j];
			if (fi.p2 == hf.p1) {
				if (hf.p2+hf.len == fi.p1) { // �n�_�̍��ׂɉ��������݂�����A���̉������E���ɉ����o���Ȃ����`�F�b�N
					;
				}else if (hf.p2-1 == fi.p1) { // �n�_�̉E�ׂɉ��������݂�����A���̉����������ɉ����o���Ȃ����`�F�b�N
					uint8_t newLenBits = calcNumBits(hf.len+1);
					if (newLenBits <= hMaxLenBitLen) {
						--hf.p2;
						++hf.len;
						hMaxLen = std::max(hMaxLen, hf.len); // not sure if this was the longest hLine though.
						++fi.p2;
						--fi.len;
					}
				}
			}
			uint8_t ep = fi.p2 + fi.len-1;
			if (ep == hf.p1) {
				if (fi.p1+1 == hf.p2) { // �I�_�̉E�ׂɉ��������݂�����A���̉����������ɉ����o���Ȃ����`�F�b�N
					uint8_t newLenBits = calcNumBits(hf.len+1);
					if (newLenBits <= hMaxLenBitLen) {
						--hf.p2;
						++hf.len;
						hMaxLen = std::max(hMaxLen, hf.len); // not sure if this was the longest hLine though.
						--fi.len;
					}
				}else if (fi.p1 == hf.p2+hf.len) { // �I�_�̍��ׂɉ��������݂�����A���̉������E���ɉ����o���Ȃ����`�F�b�N
					uint8_t newLenBits = calcNumBits(hf.len+1);
					if (newLenBits <= hMaxLenBitLen) {
						++hf.len;
						hMaxLen = std::max(hMaxLen, hf.len); // not sure if this was the longest hLine though.
						--fi.len;
					}
				}
			}
		}
	}


}

std::string Encode(const BitmapFont& bf, BitWriter& bw)
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	
	searchFills(bf, hFills, vFills);
	optimizeFills(hFills, vFills);
	std::sort(vFills.begin(), vFills.end());
	
	std::vector<std::string> cmds;
	char buff[32];
	sprintf(buff, "(%d %d %d %d)", bf.x_, bf.y_, bf.w_, bf.h_);
	cmds.push_back(buff);

	encodeNum(bw, bf.x_, 16);
	encodeNum(bw, bf.y_, 16);
	assert(bf.w_ != 0 && bf.h_ != 0);
	encodeNum(bw, 16-(bf.x_+bf.w_), 16-bf.x_);
	encodeNum(bw, 16-(bf.y_+bf.h_), 16-bf.y_);

	buildCommands(bw, cmds, hFills, bf.h_, bf.w_);
	buildCommands(bw, cmds, vFills, bf.w_, bf.h_);
	
	std::string ret;
	for (size_t i=0; i<cmds.size(); ++i) {
		ret += cmds[i];
		ret += "\r\n";
	}
	
	return ret;
}

