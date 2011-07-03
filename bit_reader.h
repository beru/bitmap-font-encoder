#ifndef BIT_READER_H_INCLUDED__
#define BIT_READER_H_INCLUDED__

class BitReader
{
public:
	void Set(const uint8_t* src)
	{
		src_ = src;
		initialSrc_ = src;
		counter_ = 0;

		totalCounter_ = 0;
	}
	
	bool Front()
	{
		return *src_ & (1 << (7-counter_));
	}

	bool Pop()
	{
		++totalCounter_;
		bool ret = Front();
		++counter_;
		if (counter_ == 8) {
			counter_ = 0;
			++src_;
		}
		return ret;
	}
	
	size_t nBytes() const { return (src_ - initialSrc_) + (counter_ ? 1 : 0); }
	
private:
	uint32_t totalCounter_;
	unsigned char counter_;
	const uint8_t* src_;
	const uint8_t* initialSrc_;
};

template <typename T>
void popBits(BitReader& br, T& val)
{
	for (uint8_t i=0; i<sizeof(T)*8; ++i) {
		val |= br.Pop() << i;
	}
}

#endif // #ifndef BIT_READER_H_INCLUDED__

