#include "bmp_font_encode.h"

#include <algorithm>
#include "bit_writer.h"
#include "misc.h"
#include "integer_coding.h"

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
	enum Direction {
		Direction_Left,
		Direction_Right,
	} dir;
};

#define PIXEL_X 2
#define PIXEL_Y 4

bool operator < (const FillInfo& lhs, const FillInfo& rhs)
{
	return lhs.p1 < rhs.p1;
}

extern uint32_t g_dist[17][17][17];

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

static
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
		for (uint8_t i=0; i<fontInfo.maxW; ++i) {
			recLineEmptyFlag(1, bw, false);
		}
		return;
	}
	
	// �ő�����̋L�^
	uint8_t maxLen = 1;
	uint8_t maxLen2 = 0;
	{
		// ��s���ǂ����̋L�^
		uint32_t lineFlags = 0;
		for (size_t i=0; i<vFills.size(); ++i) {
			const FillInfo& fi = vFills[i];
			maxLen = std::max(maxLen, fi.len);
			lineFlags |= 1 << fi.p1;
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
//TRACE("%x", (lineFlags & (1<<i)) != 0);
		}
//TRACE("\r\n");
	}
	// �ő�����̋L�^
	// TODO: �f�[�^�L���s�̈�ԏ��߂̓h��Ԃ��̊J�n�ʒu���L�^����΁A�͈͂����߂���B
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

// �㉺�ɘA�����Ă��Ȃ��P��Y�s�N�Z�����ǂ���
static
bool isSingleYPixel(
	const Array2D<uint8_t>& values,
	uint8_t x, uint8_t y
	)
{
	if (values[y][x] != PIXEL_Y) {
		return false;
	}
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	if (y != 0) {
		if (values[y-1][x]) {
			return false;
		}
	}
	if (y != h-1) {
		if (values[y+1][x]) {
			return false;
		}
	}
	return true;
}

static
uint8_t findSlantingDotsToTheLeft(
	const Array2D<uint8_t>& values,
	uint8_t x, uint8_t y
	)
{
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	
	assert(x > 0 & x < w);
	assert(y >= 0 && y < h - 1);
	
	size_t xRemain = x + 1;
	size_t yRemain = h - y;

	uint8_t repLen = 0;
	for (size_t i=0; i<std::min(xRemain, yRemain); ++i) {
		if (!isSingleYPixel(values, x-i, y+i)) {
			break;
		}
		++repLen;
	}
	
	return repLen;
}

static
uint8_t findSlantingDotsToTheRight(
	const Array2D<uint8_t>& values,
	uint8_t x, uint8_t y
	)
{
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	
	assert(x >= 0 & x < w - 1);
	assert(y >= 0 && y < h - 1);
	
	size_t xRemain = w - x;
	size_t yRemain = h - y;
	
	uint8_t repLen = 0;
	for (size_t i=0; i<std::min(xRemain, yRemain); ++i) {
		if (!isSingleYPixel(values, x+i, y+i)) {
			break;
		}
		++repLen;
	}
	
	return repLen;
}


struct SlantingDotsInfo
{
	enum Direction {
		Direction_ToTheLeft,
		Direction_ToTheRight,
	} dir;
	uint8_t x;
	uint8_t y;
	uint8_t len;
};

static
void findSlangtingDots(
	const Array2D<uint8_t>& values,
	const BmpFontHeader& fontInfo
	)
{
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();

	if (h < 2 | w < 2) {
		return;
	}

//	std::pair<uint8_t, uint8_t> pos;
	std::vector<SlantingDotsInfo> repeats;
	uint8_t repeatLen = 0;
	
	// ���������̐������̗񂩂�E�̗�ɂȂ����ĒT��
	for (uint8_t x=1; x<w; ++x) {
		for (uint8_t i=0; i<std::min((uint8_t)(h-1), x); ++i) {
			uint8_t x2 = x - i;
			uint8_t y2 = i;
			repeatLen = findSlantingDotsToTheLeft(values, x2, y2);
			if (repeatLen >= 3) {
				SlantingDotsInfo info;
				info.dir = SlantingDotsInfo::Direction_ToTheLeft;
				info.x = x2;
				info.y = y2;
				info.len = repeatLen;
				repeats.push_back(info);
			}
			i+=repeatLen;
		}
	}
	// �E�[�ɍs�����̂ŉ��ɉ������Ă����B
	for (uint8_t y=1; y<h-1; ++y) {
		for (uint8_t i=0; i<std::min(w-1, h-1-y); ++i) {
			uint8_t x2 = w-1 - i;
			uint8_t y2 = y + i;
			repeatLen = findSlantingDotsToTheLeft(values, x2, y2);
			if (repeatLen >= 3) {
				SlantingDotsInfo info;
				info.dir = SlantingDotsInfo::Direction_ToTheLeft;
				info.x = x2;
				info.y = y2;
				info.len = repeatLen;
				repeats.push_back(info);
			}
			i+=repeatLen;
		}
	}
	
	// �E�������̐����E���獶�ɂȂ����ĒT��
	for (uint8_t x=1; x<w; ++x) {
		for (uint8_t i=0; i<std::min((uint8_t)(h-1), x); ++i) {
			uint8_t x2 = w - 1 - x + i;
			uint8_t y2 = i;
			repeatLen = findSlantingDotsToTheRight(values, x2, y2);
			if (repeatLen >= 3) {
				SlantingDotsInfo info;
				info.dir = SlantingDotsInfo::Direction_ToTheRight;
				info.x = x2;
				info.y = y2;
				info.len = repeatLen;
				repeats.push_back(info);
			}
			i+=repeatLen;
		}
	}
	// ���[�ɍs�����̂ŉ��ɉ������Ă���
	for (uint8_t y=1; y<h-1; ++y) {
		for (uint8_t i=0; i<std::min(w-1, h-1-y); ++i) {
			uint8_t x2 = i;
			uint8_t y2 = y + i;
			repeatLen = findSlantingDotsToTheRight(values, x2, y2);
			if (repeatLen >= 3) {
				SlantingDotsInfo info;
				info.dir = SlantingDotsInfo::Direction_ToTheRight;
				info.x = x2;
				info.y = y2;
				info.len = repeatLen;
				repeats.push_back(info);
			}
			i+=repeatLen;
		}
	}
	
}

static
void buildSlantingCommands(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo,
	std::vector<SlantingFillInfo>& sFills
	)
{
}

static
void buildHorizontalCommands(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo,
	const std::vector<FillInfo>& fills
	)
{
	if (fills.size() == 0) {
		// �S�����s�I
		for (uint8_t i=0; i<fontInfo.maxH; ++i) {
			recLineEmptyFlag(0, bw, false);
		}
		return;
	}
	
	uint8_t maxLen = 1; // �h��Ԃ��ő咷
	// ��s���ǂ����̋L�^
	{
		uint16_t lineFlags = 0;
		for (size_t i=0; i<fills.size(); ++i) {
			const FillInfo& fi = fills[i];
			maxLen = std::max(maxLen, fi.len);
			lineFlags |= 1 << fi.p1;
		}
		for (uint8_t i=0; i<bf.h_; ++i) {
			recLineEmptyFlag(0, bw, lineFlags & (1<<i));
		}
	}
	
	// TODO: �f�[�^�L���s�̈�ԏ��߂̓h��Ԃ��̊J�n�ʒu���ɋL�^����΁A�͈͂����߂���B
	// �ő�����̋L�^
	assert(maxLen >= 2);
	encode_CBT(DataType_X_MaxLen, bw, maxLen-2, bf.w_-1);
	
	uint8_t col = 0;
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
			encode_CBT(DataType_X_Len, bw, fi.len-2, std::min(maxLen, remain)-2+1);
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
		++cnt;
		if (cnt == vy) {
			break;
		}
	}
	return y;
}

void searchFills(
	Array2D<uint8_t>& values,
	std::vector<FillInfo>& hFills,
	std::vector<FillInfo>& vFills,
	std::vector<SlantingFillInfo>& sFills,
	uint8_t* vlens
	)
{
	searchHorizontalFills(values, hFills);
	searchVerticalFills(values, vFills, vlens);
	
	sFills.clear();
	
	const uint8_t w = values.GetWidth();
	const uint8_t h = values.GetHeight();
	
	// 2�h�b�g�ȏ�̎΂ߐ����Ώ�
	
	// �������̐����E�[���璲�ׂ�
	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[vFills.size()-1-i];
		uint8_t x = fi.p1;
		uint8_t y = vFillYtoOrgY(values, x, fi.p2 + fi.len);
		// �������㔼2�s�̏ꍇ�́A2�h�b�g�ȏ�̎΂ߐ��������Ȃ��̂őΏۊO�Ƃ���B
		if (y >= h-2) {
			continue;
		}
		uint8_t nRepeats;
		if (fi.p1 > 1) {
			nRepeats = findSlantingDotsToTheLeft(values, x-1, y+1);
			if (nRepeats >= 2) {
				uint8_t hoge = 0;
			}
		}
		
	}

	for (size_t i=0; i<vFills.size(); ++i) {
		const FillInfo& fi = vFills[i];
		uint8_t x = fi.p1;
		uint8_t y = vFillYtoOrgY(values, x, fi.p2 + fi.len);
		// �������㔼2�s�̏ꍇ�́A2�h�b�g�ȏ�̎΂ߐ��������Ȃ��̂őΏۊO�Ƃ���B
		if (y >= h-2) {
			continue;
		}
		uint8_t nRepeats;
		if (fi.p1 < w-2) {
			nRepeats = findSlantingDotsToTheRight(values, x+1, y+1);
			if (nRepeats >= 2) {
				uint8_t hoge = 0;
			}
		}
		int hoge = 0;
	}
	
}

void push16(BitWriter& bw, uint16_t u)
{
	for (uint8_t i=0; i<16; ++i) {
		bw.Push(u & (1<<i));
	}
}

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

void Encode(
	BitWriter& bw,
	const BitmapFont& bf,
	const BmpFontHeader& fontInfo
	)
{
	std::vector<FillInfo> hFills;
	std::vector<FillInfo> vFills;
	std::vector<SlantingFillInfo> sFills;
	
	Array2D<uint8_t> values = bf.values_;
	uint8_t vlens[32]; // �c�L�^�̗̈�̍����A���L�^�����������肳���
	searchFills(values, hFills, vFills, sFills, vlens);
	std::sort(hFills.begin(), hFills.end());
	std::sort(vFills.begin(), vFills.end());
	
	std::vector<std::string> cmds;
	char buff[32];
	sprintf(buff, "(%d %d %d %d)", bf.x_, bf.y_, bf.w_, bf.h_);
	cmds.push_back(buff);
	
	uint8_t recX = bf.x_ - fontInfo.minX;
	uint8_t recY = bf.y_ - fontInfo.minY;
	uint8_t recW = fontInfo.maxW - (bf.x_ + bf.w_);
	uint8_t recH = fontInfo.maxH - (bf.y_ + bf.h_);
	// TODO: 0���1�����|�I�ɑ����̂Łc�B�B�����v�K�؂ɑΏ�
	recX = (recX == 0 ? 15 : recX-1);
	encode_Alpha(0, bw, recX);
	encode_Alpha(1, bw, recY);
//	assert(bf.w_ != 0 && bf.h_ != 0);
	encode_Alpha(2, bw, recW);
	encode_Alpha(3, bw, recH);
	
	buildHorizontalCommands(bw, bf, fontInfo, hFills);
	buildVerticalCommands(bw, bf, fontInfo, vFills, hFills, bf.w_, vlens);
	buildSlantingCommands(bw, bf, fontInfo, sFills);


}

