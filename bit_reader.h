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
	}
	
	bool Pop()
	{
		bool ret = *src_ & (1 << (7-counter_));
		++counter_;
		if (counter_ == 8) {
			counter_ = 0;
			++src_;
		}
		return ret;
	}
	
	size_t nBytes() const { return (src_ - initialSrc_) + (counter_ ? 1 : 0); }
	
private:
	unsigned char counter_;
	const uint8_t* src_;
	const uint8_t* initialSrc_;
};

#endif // #ifndef BIT_READER_H_INCLUDED__

