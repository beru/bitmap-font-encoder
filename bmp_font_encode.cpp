#include "bmp_font_encode.h"

#include <algorithm>
#include "bit_writer.h"
#include "misc.h"
#include "integer_coding.h"

extern uint32_t g_dist[17][33][33];

namespace {

struct FillInfo
{
	uint8_t p1;
	uint8_t p2;
	uint8_t len;
};

struct SlantingFillInfo
{
	uint8_t x;
	uint8_t y;
	uint8_t len;
	uint8_t vFillIndex;
	enum Direction {
		Direction_Left,
		Direction_Right,
	} dir;
};

#define PIXEL_X 2
#define PIXEL_Y 4
#define PIXEL_UNDELETABLE 8

bool operator < (const FillInfo& lhs, const FillInfo& rhs)
{
	return lhs.p1 < rhs.p1;
}

bool operator < (const SlantingFillInfo& lhs, const SlantingFillInfo& rhs)
{
	if (lhs.vFillIndex == rhs.vFillIndex) {
		return lhs.dir < rhs.dir;
	}else {
		return lhs.vFillIndex < rhs.vFillIndex;
	}
}

void recLineEmptyFlag(uint8_t type, BitWriter& bw, bool isNotEmpty)
{
	++g_dist[8][type][isNotEmpty];
	bw.Push(isNotEmpty);
}

enum DataType {
	DataType_Region = -100,
	DataType_X_MaxLen = 0,
	DataType_X_Offset,
	DataType_X_Len,
	DataType_Y_MaxLen,
	DataType_Y_Offset,
	DataType_Y_Len,
	DataType_S_Idx,
};

void encode_CBT(DataType type, BitWriter& bw, uint8_t n, uint8_t m)
{
	integerEncode_CBT(bw, n, m);
	if (type < 0) {
		return;
	}
	++g_dist[type][m][n];
}

void encode_Alpha(uint8_t type, BitWriter& bw, uint8_t n)
{
	integerEncode_Alpha(bw, n);
	++g_dist[7][type][n];
}

uint32_t makeLineEmptyFlags(const std::vector<FillInfo>& fills)
{
	uint16_t ret = 0;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		ret |= 1 << fi.p1;
	}
	return ret;
}

void buildVerticalCommands(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo,
	const std::vector<FillInfo>& vFills,
	const std::vector<FillInfo>& hFills,
	uint8_t len1,
	const uint8_t* len2s
	)
{
	if (vFills.size() == 0) {
		// �S�����s�I
		for (uint8_t i=0; i<bf.w_; ++i) {
			recLineEmptyFlag(1, bw, false);
		}
		return;
	}
	
	// �ő�����̋L�^
	uint8_t maxLen = 1;
	uint8_t maxLen2 = 0;
	{
		// ��s���ǂ����̋L�^
		for (size_t i=0; i<vFills.size(); ++i) {
			const FillInfo& fi = vFills[i];
			maxLen = std::max(maxLen, fi.len);
			maxLen2 = std::max(maxLen2, len2s[fi.p1]);
		}

		// �c���J�n�ʒu�ƕ��A�������L�^�����ꍇ�A���ʂō��[�̍s��E�[�̍s�ɋL�^�����������ꍇ��
		// �c�ʂ̋L�^�œh��Ԃ������݂���̂͊m���Ȃ̂ŁA���s�L�^�����̕��ȗ��ł���B
		uint8_t beginX = -1;
		uint8_t endX = 0;
		for (size_t i=0; i<hFills.size(); ++i) {
			const FillInfo& fi = hFills[i];
			beginX = std::min(beginX, fi.p2);
			endX = std::max(endX, (uint8_t)(fi.p2+fi.len));
		}
		uint32_t lineFlags = makeLineEmptyFlags(vFills);
#if 1
		for (uint8_t i=0; i<bf.w_; ++i) {
			recLineEmptyFlag(1, bw, lineFlags & (1<<i));
		}
#else
		bool skipFirst = (beginX > 0);
		bool skipLast = (endX < len1);
		for (uint8_t i=0; i<bf.w_; ++i) {
			if (i== 0 && skipFirst) {
				continue;
			}
			if (i==bf.w_-1 && skipLast) {
				continue;
			}
			recLineEmptyFlag(1, bw, lineFlags & (1<<i));
		}
#endif
	}
	// �ő�����̋L�^
	// TODO: �f�[�^�L���s�̈�ԏ��߂̓h��Ԃ��̊J�n�ʒu���ɋL�^����΁A�͈͂����߂���B
	encode_CBT(DataType_Y_MaxLen, bw, maxLen-1, maxLen2);
	
	uint8_t col = 0;
	uint8_t row = vFills[0].p1;
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		if (row != fi.p1) {
			if (col < len2s[row]) {
				bw.Push(false);
			}
			col = 0;
		}else {
			if (col != 0) {
				bw.Push(true); // fill sign
			}
		}
		row = fi.p1;
		uint8_t len2 = len2s[row];
		uint8_t offset = fi.p2 - col;
		uint8_t remain = len2 - fi.p2;
		if (remain < 2) {
			if (offset == 0) {
				// offset == 0 do not record
			}else {
				encode_CBT(DataType_Y_Offset, bw, offset, len2-col);
			}
			assert(fi.len == 1);
			// len == 1 do not record
		}else {
			assert(fi.len >= 1);
			encode_CBT(DataType_Y_Offset, bw, offset, len2-col);
			encode_CBT(DataType_Y_Len, bw, fi.len-1, std::min(maxLen, remain));
		}

		if (col == 0) {
			col = fi.p2;
		}else {
			col += offset;
		}
		col += fi.len + 1;
	}
	if (col < len2s[row]) {
		bw.Push(false);
	}
}

void buildHorizontalCommands(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo,
	const std::vector<FillInfo>& fills
	)
{
	if (fills.size() == 0) {
		// �S�����s�I
		for (uint8_t i=0; i<bf.h_; ++i) {
			recLineEmptyFlag(0, bw, false);
		}
		return;
	}
	
	uint8_t maxLen = 2; // �h��Ԃ��ő咷
	// ��s���ǂ����̋L�^
	{
		for (size_t i=0; i<fills.size(); ++i) {
			const FillInfo& fi = fills[i];
			if (fi.len != 0xFF) {
				maxLen = std::max(maxLen, fi.len);
			}
		}
		uint32_t lineFlags = makeLineEmptyFlags(fills);
		for (uint8_t i=0; i<bf.h_; ++i) {
			recLineEmptyFlag(0, bw, lineFlags & (1<<i));
		}
	}
	
	// TODO: �f�[�^�L���s�̈�ԏ��߂̓h��Ԃ��̊J�n�ʒu���ɋL�^����΁A�͈͂����߂���B
	// �ő�����̋L�^
	encode_CBT(DataType_X_MaxLen, bw, maxLen-2, bf.w_-1);
	
	uint16_t col = 0;
	uint8_t row = fills[0].p1;
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		if (row != fi.p1) {
			assert(row < fi.p1);
			if (col <= bf.w_-2) {
				bw.Push(false);
			}
			col = 0;
		}else {
			assert(col <= fi.p2);
			if (col != 0) {
				bw.Push(true); // fill sign
			}
		}

		row = fi.p1;
		uint8_t offset = fi.p2 - col;
		
		assert(fi.p2 <= bf.w_-2);
		uint8_t remain = bf.w_ - fi.p2;
		if (remain <= 2) {
			// TODO: ���h��Ԃ��̒������ŏ�2�h�b�g�Ȃ�Β�������ɋL�^����K�v�͖����͂��B��������1�h�b�g�������Ă���H
			if (offset == 0) {
				// offset == 0 do not record
			}else {
				encode_CBT(DataType_X_Offset, bw, offset, bf.w_-1-col);
			}
			assert(fi.len == 2);
			// len == 1 do not record
		}else {
			assert(fi.len >= 2);
			encode_CBT(DataType_X_Offset, bw, offset, bf.w_-1-col);
			uint8_t limit = std::min(remain, maxLen);
			// �����܂ł̓h��Ԃ�����ʎ�
			// ���h��Ԃ��͍ŏ�������2�h�b�g�Ȃ̂ŁA�ő咷��-2�܂ł̋L�^�ōςނ���ǁA�ő咷��-1�𖖔��܂ł̓h��Ԃ��Ɋ��蓖�Ă�B
			// �����܂ł̓h��Ԃ�����ʂɊ��蓖�Ă鎖�őS�̂̋L�^����Z������B
			if (fi.len == 0xFF) {
				encode_CBT(DataType_X_Len, bw, limit-1, limit);
			}else {
				encode_CBT(DataType_X_Len, bw, fi.len-2, limit);
			}
		}
		
		if (col == 0) {
			col = fi.p2;
		}else {
			col += offset;
		}
		col += fi.len + 1;
	}
	if (col <= bf.w_-2) {
		bw.Push(false);
	}
}

uint8_t vFillYtoOrgY(const Array2D<uint8_t>& values, uint8_t x, uint8_t vy)
{
	// vFill y to original Y
	uint8_t cnt = 0;
	uint8_t y;
	const uint8_t h = values.GetHeight();
	for (y=0; y<h; ++y) {
		uint8_t v = values[y][x];
		if (v == PIXEL_X) {
			continue;
		}
		if (cnt == vy) {
			break;
		}
		++cnt;
	}
	return y;
}

// �㉺�ɘA�����Ă��Ȃ��P��Y�s�N�Z�����ǂ���
bool isSingleYPixel(
	const Array2D<uint8_t>& values,
	std::vector<FillInfo>& vFills,
	uint8_t x, uint8_t y
	)
{
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		uint8_t x2 = fi.p1;
		if (x2 != x || fi.len != 1) {
			continue;
		}
		uint8_t y2 = vFillYtoOrgY(values, x2, fi.p2);
		if (y2 == y) {
			return true;
		}
	}
	return false;
}

void buildSlantingCommands(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo,
	const std::vector<FillInfo>& vFills,
	std::vector<SlantingFillInfo>& sFills
	)
{
	if (!vFills.size()) {
		return;
	}
	
	// vFill�̒���SlantingFill��h���o������̂͌����Ă���̂ŁA��␔�����点��B
	std::vector<uint8_t> idxList;
	idxList.reserve(vFills.size());
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		uint8_t x2 = fi.p1;
		uint8_t y2 = vFillYtoOrgY(bf.values_, x2, fi.p2+fi.len-1);
		if (y2 == bf.h_-1) {
			continue;
		}
		bool bLeftAvail = true;
		bool bRightAvail = true;
		if (x2 == 0) {
			bLeftAvail = false;
		}else if (bf.values_[y2+1][x2-1] == PIXEL_X) {
			bLeftAvail = false;
		}else {
			for (size_t j=0; j<vFills.size(); ++j) {
				const FillInfo& fi2 = vFills[j];
				if (fi2.p1 == x2-1 && fi2.len != 1 && vFillYtoOrgY(bf.values_, fi2.p1, fi2.p2) == y2+1) {
					bLeftAvail = false;
					break;
				}
			}
		}
		if (x2 == bf.w_-1) {
			bRightAvail = false;
		}else if (bf.values_[y2+1][x2+1] == PIXEL_X) {
			bRightAvail = false;
		}else {
			for (size_t j=0; j<vFills.size(); ++j) {
				const FillInfo& fi2 = vFills[j];
				if (fi2.p1 == x2+1 && fi2.len != 1 && vFillYtoOrgY(bf.values_, fi2.p1, fi2.p2) == y2+1) {
					bRightAvail = false;
					break;
				}
			}
		}
		if (!bLeftAvail && !bRightAvail) {
			continue;
		}
		idxList.push_back(i);
	}
	
	// �������ƉE���������邪�A�E�����������玟��vFill�Ɉڂ�Ƃ������[��
	// ����vFill�ɑ΂��č������ƉE���������s���ꍇ�́A�����������Ă���E����������B
	
	size_t idx = 0;
	uint8_t prevIndex = -1;
	for (size_t i=0; i<sFills.size(); ++i) {
		const SlantingFillInfo& fi = sFills[i];
		uint8_t ci = std::find(idxList.begin(), idxList.end(), fi.vFillIndex) - idxList.begin();
		encode_CBT(DataType_S_Idx, bw, (ci - idx)+1, idxList.size()-idx+1);
		
		bool bRecDir = true;
		// �������ɍ��������Ă�̂ł���΁A�����L�^����K�v�������B�i�E��������Ȃ̂Łj
		if (ci == prevIndex) {
			bRecDir = false;
		}
		// ���[��E�[�̏ꍇ�͉����o������������肳���̂ŕ����L�^����K�v�������B
		if (fi.x == 0 && fi.x == bf.w_-1)	{
			bRecDir = false;
		}
		// �����ł�����������肳��Ă��Ȃ��������i�c�h��Ԃ��������ǂ��ł��Ȃ����j
		for (size_t j=0; j<vFills.size(); ++j) {
			if (j == fi.vFillIndex) {
				continue;
			}
			const FillInfo& vf = vFills[j];
			uint8_t x2 = vf.p1;
			uint8_t y2 = vFillYtoOrgY(bf.values_, x2, vf.p2);
			if (fi.y+1 == y2 && (fi.x-1 == x2 || fi.x+1 == x2)) {
				bRecDir = false;
				break;
			}
		}
		if (bRecDir) {
			bw.Push(fi.dir);
		}
		idx = ci;
		// �E�������������Ă���̂ł���΁A����vFill���琔�����J�n�o����B
		if (fi.dir == SlantingFillInfo::Direction_Right) {
			++idx;
		}
		prevIndex = ci;
	}
	
	// �I���R�[�h

	// �Ō��vFill�̉E�����������Ȃ�΁A�I���R�[�h���ȗ��o����B
	if (sFills.size() == 0 || idx != vFills.size()) {
		encode_CBT(DataType_S_Idx, bw, 0, vFills.size()-idx+1);
	}
}

bool isLineFullfilled(const std::vector<FillInfo>& fills, uint8_t p1, uint8_t w)
{
	for (size_t i=0; i<fills.size(); ++i) {
		const FillInfo& fi = fills[i];
		if (fi.p1 == p1 && fi.len == w) {
			return true;
		}
	}
	return false;
}

uint8_t findXRepeatLength(const Array2D<uint8_t>& values, uint8_t x, uint8_t y)
{
	if (!values[y][x]) {
		return 0;
	}
	uint8_t len = 1;
	for (int i=x-1; i!=-1; --i) {
		if (!values[y][i]) {
			break;
		}
		++len;
	}
	for (int i=x+1; i<values.GetWidth(); ++i) {
		if (!values[y][i]) {
			break;
		}
		++len;
	}
}

uint8_t findYRepeatLength(const Array2D<uint8_t>& values, uint8_t x, uint8_t y)
{
	if (!values[y][x]) {
		return 0;
	}
	uint8_t len = 1;
	for (int i=y-1; i!=-1; --i) {
		if (!values[i][x]) {
			break;
		}
		++len;
	}
	for (int i=y+1; i<values.GetHeight(); ++i) {
		if (!values[i][x]) {
			break;
		}
		++len;
	}
	return len;
}

void searchHorizontalFills(
	Array2D<uint8_t>& values,
	std::vector<FillInfo>& hFills
	)
{
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	
	hFills.clear();
	
	// �������̐��̊��S�h��Ԃ����ŏ��ɒ���
	if (w > 1) {
		for (uint8_t y=0; y<h; ++y) {
			uint8_t x;
			for (x=0; x<w; ++x) {
				if (!values[y][x]) {
					break;
				}
			}
			if (x != w) {
				continue;
			}
			FillInfo fi;
			fi.p1 = y;
			fi.p2 = 0;
			fi.len = w;
			hFills.push_back(fi);

			for (x=0; x<w; ++x) {
				values[y][x] = PIXEL_X;
			}
		}
	}
	// �������̓h��Ԃ������W
	for (uint8_t y=0; y<h; ++y) {
		// ���S�ɓh��Ԃ�����Ă�s�͔�΂�
		if (isLineFullfilled(hFills, y, w)) {
			continue;
		}
		
		uint8_t* line = values[y];
		for (uint8_t x=0; x<w; ++x) {
			if (!line[x]) {
				continue;
			}
			// �h�b�g���������ꍇ
			uint8_t ex;
			// �������̘A������
			for (ex=x+1; ex<w; ++ex) {
				if (!line[ex]) {
					break;
				}
			}
			if (ex-x > 1) {
				// ���̘A���h��Ԃ�

				// �n�_�������c���Ɋ܂܂�Ă���ꍇ�͒���������
				if (findYRepeatLength(values, x, y) > ex-x) {
					++x;
				}
				// �I�_�������c���Ɋ܂܂�Ă���ꍇ�͒���������
				if (findYRepeatLength(values, ex-1, y) > ex-x) {
					// �������I�[�̂P��O�܂ŗ��Ă���ꍇ�͉��s����ߖ�ł���̂Ō������Ȃ��B
					if (ex != w-1) {
						--ex;
					}
				}
				if (ex-x > 1) {
					FillInfo fi;
					fi.p1 = y;
					fi.p2 = x;
					fi.len = ex - x;
					hFills.push_back(fi);
					for (uint8_t i=x; i<ex; ++i) {
						line[i] = PIXEL_X;
					}
					x = ex;
					continue;
				}
			}

			//// ��������1pixel ////
			continue;
		}
	}

#if 1
	// �������̓h��Ԃ��̍œK��
	uint8_t maxLen = 0;
	for (size_t i=0; i<hFills.size(); ++i) {
		maxLen = std::max(maxLen, hFills[i].len);
	}
	uint8_t lp1 = -1;
	for (size_t i=0; i<hFills.size(); ++i) {
		FillInfo& fi = hFills[i];
		// �Ƃ肠�����s���̍ŏ��̓h��Ԃ������Ώۂɂ���B
		if (fi.p1 == lp1) {
			continue;
		}
		lp1 = fi.p1;
		
		if (fi.p2 > 0) {
			// �n�_�̍����ɏc�����̓h��Ԃ�������ꍇ�A�������ɉ������Ă��e�ʂ������Ȃ����ǂ�������
			if (values[fi.p1][fi.p2-1]) {
				assert(values[fi.p1][fi.p2-1] == 1);
				uint8_t oldBitLen = calcIntegerEncodedLength_CBT(fi.p2, w-1) + calcIntegerEncodedLength_CBT(fi.len-1, std::min(maxLen, (uint8_t)(w-fi.p2)));
				uint8_t newP2 = fi.p2 - 1;
				uint8_t newLen = fi.len + 1;
				if (newLen <= maxLen) {
					uint8_t newBitLen = calcIntegerEncodedLength_CBT(newP2, w) + calcIntegerEncodedLength_CBT(newLen-1, std::min(maxLen, (uint8_t)(w-newP2)));
					if (newBitLen <= oldBitLen) {
						values[fi.p1][fi.p2-1] = PIXEL_X;
						--fi.p2;
						++fi.len;
					}
				}
			}
		}
		if (fi.p2+fi.len < w) {
			// �I�_�̉E���ɏc�����̓h��Ԃ�������ꍇ�A�E�����ɉ������Ă��e�ʂ������Ȃ����ǂ�������
			if (values[fi.p1][fi.p2+fi.len]) {
				assert(values[fi.p1][fi.p2+fi.len] == 1);
				uint8_t oldBitLen = calcIntegerEncodedLength_CBT(fi.p2, w-1) + calcIntegerEncodedLength_CBT(fi.len-2, std::min(maxLen, (uint8_t)(w-fi.p2))-1);
				uint8_t newLen = fi.len + 1;
				if (newLen <= maxLen) {
					uint8_t newBitLen = calcIntegerEncodedLength_CBT(fi.p2, w-1) + calcIntegerEncodedLength_CBT(newLen-2, std::min(maxLen, (uint8_t)(w-fi.p2))-1);
					if (newBitLen <= oldBitLen) {
						assert(newBitLen == oldBitLen);
						values[fi.p1][fi.p2+fi.len] = PIXEL_X;
						++fi.len;
					}
				}
			}
		}
	}
#endif
	
	// �I�[�i�E�[�j�܂ł̓h��Ԃ�����ʎ�
	for (size_t i=0; i<hFills.size(); ++i) {
		FillInfo& fi = hFills[i];
		if (fi.p2 + 2 < w) { // �Z���͖̂���
			if (fi.p2 + fi.len == w) {
				fi.len = 0xFF;
			}
		}
	}
}

void searchVerticalFills(
	Array2D<uint8_t>& values,
	std::vector<FillInfo>& vFills,
	uint8_t* vlens
	)
{
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	
	vFills.clear();
	
	// �c�����̓h��Ԃ�
	for (uint8_t x=0; x<w; ++x) {
		uint8_t cnt = 0;
		bool yBuff[32] = {false};
		// X�L�^����菜�����c���������W
		for (uint8_t y=0; y<h; ++y) {
			uint8_t v = values[y][x];
			if (v != PIXEL_X) {
				if (v) {
					yBuff[cnt] = true;
					values[y][x] = PIXEL_Y;
				}
				++cnt;
			}
		}
		vlens[x] = cnt;
		// �c���A���̏����L�^
		for (uint8_t y=0; y<cnt; ++y) {
			if (yBuff[y]) {
				uint8_t ey;
				for (ey=y+1; ey<cnt; ++ey) {
					if (!yBuff[ey]) {
						break;
					}
				}
				FillInfo fi;
				fi.p1 = x;
				fi.p2 = y;
				fi.len = ey - y;
				vFills.push_back(fi);
				y = ey;
			}
		}
	}
}

uint8_t findVFillIndexByTailPos(const Array2D<uint8_t>& values, const std::vector<FillInfo>& vFills, uint8_t x, uint8_t y)
{
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		if (fi.p1 != x) {
			continue;
		}
		if (vFillYtoOrgY(values, x, fi.p2+fi.len-1) == y) {
			return i;
		}
	}
	return -1;
}

void searchFills(
	Array2D<uint8_t>& values,
	std::vector<FillInfo>& hFills,
	std::vector<FillInfo>& vFills,
	std::vector<SlantingFillInfo>& sFills,
	uint8_t* vlens
	)
{
	// �e�����ǂ̂悤��Modeling����Ă���̂����o�I�ɕ�����悤�ɂ���B
	
	// TODO: �؂Ƃ������̍��E�̎}�́A�c����1�h�b�g���琶�₹���鎖�������B���_�𖳑ʂɍ��Ȃ��B
	// �����c���̉�����}���o��ꍇ�A����2�h�b�g�����邯��ǁA���_�ɂ��Ȃ��ŁA�c���ɗאڂ���1�h�b�g�̏c�_����}�𐶂₷�悤�ɂ���B
	
	// TODO: �n�Ƃ������̌��̏㑤�̉��_���΂ߐ�����������Ȃ��悤�ɂ���B
	
	searchHorizontalFills(values, hFills);
	searchVerticalFills(values, vFills, vlens);
	
	// �΂ߐ��T������
	sFills.clear();
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	
	// �΂ߐ��ň�����̂ō��h�b�g���X�g
	std::vector<std::pair<uint8_t, uint8_t> > deleteList;

	// ���������ɐi�ސ����E�[���璲�ׂ�
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[vFills.size()-1-i];
		uint8_t x = fi.p1;
		uint8_t y = vFillYtoOrgY(values, x, fi.p2 + fi.len - 1);
		// �������Ō�̍s�̏ꍇ�́A2�h�b�g�ȏ�̎΂ߐ��������Ȃ��̂ŊJ�n�_�̑ΏۊO�Ƃ���B
		if (y >= h-1) {
			continue;
		}
		if (fi.p1 <= 0) {
			continue;
		}
		// �΂ߐ��̒��ԃh�b�g�͊J�n�_�ɂȂ�Ȃ�
		if (std::find(deleteList.begin(), deleteList.end(), std::make_pair(x, y)) != deleteList.end()) {
			continue;
		}
		size_t xRemain = x;
		size_t yRemain = h - y + 1;
		uint8_t repLen = 0;
		for (size_t j=0; j<std::min(xRemain, yRemain); ++j) {
			uint8_t x2 = x - 1 - j;
			uint8_t y2 = y + 1 + j;
			if (!isSingleYPixel(values, vFills, x2, y2)) {
				break;
			}
			if (values[y2][x2] & PIXEL_UNDELETABLE) {
				break;
			}
			++repLen;
		}
		if (repLen == 0) {
			continue;
		}else if (repLen == 1) {
			// 1�h�b�g�����L�тĂȂ��Ă������P��1�h�b�g���[�łȂ��ʒu�ɂ���ꍇ�A���Ȃ�
			if (isSingleYPixel(values, vFills, x-1, y+1) && x-1 != 0 && y+1 != h-1) {
				continue;
			}
		}
		// �΂ߐ��̎n�_���P�ƂP�h�b�g�̏ꍇ�́A��폜�Ώۂɂ���
		if (isSingleYPixel(values, vFills, x, y)) {
			values[y][x] |= PIXEL_UNDELETABLE;
		}
		// �΂ߐ��̏I�_���P��1�h�b�g�̏ꍇ�́A��폜�Ώۂɂ���
		int8_t x2 = x - repLen - 1;
		int8_t y2 = y + repLen + 1;
		if (x2 > 0 && y2 < h-1) {
			if (isSingleYPixel(values, vFills, x2, y2)) {
				values[y2][x2] |= PIXEL_UNDELETABLE;
			}
		}
		for (size_t j=0; j<repLen; ++j) {
			uint8_t x2 = x - 1 - j;
			uint8_t y2 = y + 1 + j;
			if (values[y2][x2] & PIXEL_UNDELETABLE) {
				break;
			}
			// �΂ߐ��ŕ`��o����P��1�h�b�g�́A�폜�Ώ�
			deleteList.push_back(std::make_pair(x2, y2));
		}

		SlantingFillInfo sf;
		sf.x = x;
		sf.y = y;
		sf.len = repLen;
		sf.dir = SlantingFillInfo::Direction_Left;
		sFills.push_back(sf);
	}

	// �E�������ɐi�ސ������[���璲�ׂ�
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		uint8_t x = fi.p1;
		uint8_t y = vFillYtoOrgY(values, x, fi.p2 + fi.len - 1);
		// �������Ō�̍s�̏ꍇ�́A�΂ߐ��������Ȃ��̂ŊJ�n�_�̑ΏۊO�Ƃ���B
		if (y >= h-1) {
			continue;
		}
		if (fi.p1 >= w-1) {
			continue;
		}
		// �΂ߐ��̒��ԃh�b�g�͊J�n�_�ɂȂ�Ȃ�
		if (std::find(deleteList.begin(), deleteList.end(), std::make_pair(x, y)) != deleteList.end()) {
			continue;
		}
		size_t xRemain = w - (x + 1);
		size_t yRemain = h - (y + 1);
		uint8_t repLen = 0;
		for (size_t j=0; j<std::min(xRemain, yRemain); ++j) {
			uint8_t x2 = x + 1 + j;
			uint8_t y2 = y + 1 + j;
			if (!isSingleYPixel(values, vFills, x2, y2)) {
				break;
			}
			if (values[y2][x2] & PIXEL_UNDELETABLE) {
				break;
			}
			++repLen;
		}
		if (repLen == 0) {
			continue;
		}else if (repLen == 1) {
			// 1�h�b�g�����L�тĂȂ��Ă������P��1�h�b�g���[�łȂ��ʒu�ɂ���ꍇ�A���Ȃ�
			if (x+1 != w-1 && y+1 != h-1) {
				if (isSingleYPixel(values, vFills, x+1, y+1) && values[y+2][x+2] == 0) {
					continue;
				}
			}
		}
		// �΂ߐ��̎n�_���P�ƂP�h�b�g�̏ꍇ�́A��폜�Ώۂɂ���
		if (isSingleYPixel(values, vFills, x, y)) {
			values[y][x] |= PIXEL_UNDELETABLE;
		}
		// �΂ߐ��̏I�_���P��1�h�b�g�̏ꍇ�́A��폜�Ώۂɂ���
		int8_t x2 = x + repLen + 1;
		int8_t y2 = y + repLen + 1;
		if (x2 > 0 && y2 < h-1) {
			if (isSingleYPixel(values, vFills, x2, y2)) {
				values[y2][x2] |= PIXEL_UNDELETABLE;
			}
		}
		for (size_t j=0; j<repLen; ++j) {
			uint8_t x2 = x + 1 + j;
			uint8_t y2 = y + 1 + j;
			if (values[y2][x2] & PIXEL_UNDELETABLE) {
				break;
			}
			// �΂ߐ��ŕ`��o����P��1�h�b�g�́A�폜�Ώ�
			deleteList.push_back(std::make_pair(x2, y2));
		}
		SlantingFillInfo sf;
		sf.x = x;
		sf.y = y;
		sf.len = repLen;
		sf.dir = SlantingFillInfo::Direction_Right;
		sFills.push_back(sf);
	}

	// �폜�Ώۂ̒P��1�h�b�g�̍폜
	std::vector<FillInfo> survivedVFills;
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		if (fi.len == 1) {
			uint8_t x = fi.p1;
			uint8_t y = vFillYtoOrgY(values, x, fi.p2);
			// �΂ߐ��̒��ԃh�b�g�͊J�n�_�ɂȂ�Ȃ�
			if (std::find(deleteList.begin(), deleteList.end(), std::make_pair(x, y)) != deleteList.end()) {
				continue;
			}
		}
		survivedVFills.push_back(fi);
	}
	vFills = survivedVFills;
	
	for (size_t i=0; i<sFills.size(); ++i) {
		SlantingFillInfo& fi = sFills[i];
		fi.vFillIndex = findVFillIndexByTailPos(values, vFills, fi.x, fi.y);
	}
	
	g_dist[9][0][0] += sFills.size();
	
}

void push16(BitWriter& bw, uint16_t u)
{
	for (uint8_t i=0; i<16; ++i) {
		bw.Push(u & (1<<i));
	}
}

} // namespace anonymous

std::string EncodeHeader(
	BitWriter& bw,const BmpFontHeader& header
	)
{
//	testIntergerCoding();
	
	push16(bw, header.characterCount);
	assert(header.characterCount != 0);
	uint16_t code = header.characterCodes[0];
	push16(bw, code);
	uint16_t prevCode = code;

	uint16_t dist[4096] = {0};
	for (uint16_t i=1; i<header.characterCount; ++i) {
		uint16_t code = header.characterCodes[i];
		int diff = code - prevCode;
		assert(diff > 0);
		++dist[diff];
		integerEncode_Delta(bw, diff - 1);
		prevCode = code;
	}
	
	encode_CBT(DataType_Region, bw, header.minX, 16);
	encode_CBT(DataType_Region, bw, header.minY, 16);
	encode_CBT(DataType_Region, bw, header.maxW-1, 16);
	encode_CBT(DataType_Region, bw, header.maxH-1, 16);

	char buff[64];
	sprintf(buff, "%d %d %d %d\r\n", header.minX, header.minY, header.maxW, header.maxH);
	return buff;
}

void EncodeBoxTable(class BitWriter& bw, const BoxInfo* pBoxes, uint8_t cnt, uint8_t regIdx, const BmpFontHeader& header)
{
	integerEncode_Gamma(bw, cnt);
	integerEncode_Gamma(bw, regIdx);
	for (uint8_t i=0; i<cnt; ++i) {
		if (i == regIdx) {
			continue;
		}
		const BoxInfo& bi = pBoxes[i];
		uint8_t recX = bi.x - header.minX;
		uint8_t recY = bi.y - header.minY;
		uint8_t recW = header.maxW - (bi.x + bi.w);
		uint8_t recH = header.maxH - (bi.y + bi.h);
		integerEncode_Gamma(bw, bi.bitLen-1);
		encode_Alpha(0, bw, recX);
		encode_Alpha(1, bw, recY);
		encode_Alpha(2, bw, recW);
		encode_Alpha(3, bw, recH);
	}
}

uint8_t countMaxRepBits(uint32_t flags)
{
	uint8_t cnt = 0;
	uint8_t maxCnt = 0;
	for (uint8_t i=0; i<sizeof(flags)*8; ++i) {
		if (flags & (1<<i)) {
			++cnt;
		}else {
			maxCnt = std::max(maxCnt, cnt);
			cnt = 0;
		}
	}
	maxCnt = std::max(maxCnt, cnt);
	return maxCnt;
}

void Encode(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo,
	const BoxInfo* pBoxes, uint8_t cnt,
	uint8_t regIdx // etcetera index
	)
{
	// Box�L�^
	uint8_t boxTableIdx = regIdx;
	for (uint8_t i=0; i<cnt; ++i) {
		const BoxInfo& bi = pBoxes[i];
		if (1
			&& bf.x_ == bi.x
			&& bf.y_ == bi.y
			&& bf.w_ == bi.w
			&& bf.h_ == bi.h
			)
		{
			boxTableIdx = i;
			break;
		}
	}
	// �̈�f�[�^ CHC������index�L�^
	const BoxInfo& bi = pBoxes[boxTableIdx];
	for (uint8_t i=0; i<bi.bitLen; ++i) {
		bw.Push(bi.chc & (1 << i));
	}
	// �p�x�����Ȃ��L�^�Ȃ̂ŗ̈���ʂɒ�`
	if (boxTableIdx == regIdx) {
		uint8_t recX = bf.x_ - fontInfo.minX;
		uint8_t recY = bf.y_ - fontInfo.minY;
		uint8_t recW = fontInfo.maxW - (bf.x_ + bf.w_);
		uint8_t recH = fontInfo.maxH - (bf.y_ + bf.h_);
		encode_Alpha(0, bw, recX);
		encode_Alpha(1, bw, recY);
		encode_Alpha(2, bw, recW);
		encode_Alpha(3, bw, recH);
	}
	
	// �h��Ԃ��L�^
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	std::vector<SlantingFillInfo> sFills;
	
	Array2D<uint8_t> values = bf.values_;
	uint8_t vlens[32]; // �c�L�^�̗̈�̍����A���L�^�����������肳���
	searchFills(values, hFills, vFills, sFills, vlens);
	std::sort(hFills.begin(), hFills.end());
	std::sort(vFills.begin(), vFills.end());
	std::sort(sFills.begin(), sFills.end());
	
	uint32_t lineFlagsH = makeLineEmptyFlags(hFills);
	uint32_t lineFlagsV = makeLineEmptyFlags(vFills);
	const uint8_t SHIFTS = sizeof(lineFlagsV) - bf.h_;
	lineFlagsV = ((~lineFlagsV) << SHIFTS) >> SHIFTS;
	uint32_t lineFlags = lineFlagsH | (lineFlagsV << bf.h_);
//	uint32_t lineFlags = lineFlagsV;
//	++g_dist[10][bf.w_][countBits32(lineFlags)];
	if (bf.w_+bf.h_ >= 30) {
		uint8_t nBits = countBits32(lineFlags);
		uint8_t repBits = countMaxRepBits(lineFlags);
		++g_dist[10][nBits][repBits];
		++g_dist[10][bf.w_+bf.h_][nBits];
	}
	

	buildHorizontalCommands(bw, bf, fontInfo, hFills);
	buildVerticalCommands(bw, bf, fontInfo, vFills, hFills, bf.w_, vlens);
	buildSlantingCommands(bw, bf, fontInfo, vFills, sFills);


}

