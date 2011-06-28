#ifndef BIT_WRITER_H_INCLUDED__
#define BIT_WRITER_H_INCLUDED__

class BitWriter
{
public:
	
	void Set(uint8_t* dest)
	{
		nBits_ = 0;
		dest_ = dest;
		initialDest_ = dest;
		counter_ = 0;
		*dest_ = 0;
	}
	
	void Push(bool b)
	{
		int v = b ? 1 : 0;
		int val = v << (7 - counter_);
		if (counter_ == 0) {
			*dest_ = val;
		}else {
			*dest_ |= val;
		}
		++counter_;
		if (counter_ == 8) {
			++dest_;
			counter_ = 0;
		}
		++nBits_;
	}
	
	size_t GetNBits() const { return nBits_; }
	size_t GetNBytes() const { return (dest_ - initialDest_) + (counter_ ? 1 : 0); }
private:
	size_t nBits_;
	uint8_t counter_;
	uint8_t* dest_;
	uint8_t* initialDest_;
};

#endif // #ifndef BIT_WRITER_H_INCLUDED__
