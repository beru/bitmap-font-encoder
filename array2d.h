#pragma once

#include <vector>

template <typename T>
class Array2D
{
public:
	Array2D(T* mem)
		:
		values_(mem)
	{
	}
	
	Array2D& operator = (const Array2D& a)
	{
		Resize(a.w_, a.h_);
		std::copy(a.values_, a.values_ + w_*h_, values_);
		return *this;
	}
	
	void Resize(size_t w, size_t h)
	{
		std::fill(values_, values_+w*h, T());
		w_ = w;
		h_ = h;
	}
	
	T* operator[] (int row) {
		return &values_[row * w_];
	}
	
	const T* operator[] (int row) const {
		return &values_[row * w_];
	}
	
	size_t GetWidth() const { return w_; }
	size_t GetHeight() const { return h_; }
	
private:
	size_t w_;
	size_t h_;
	T* values_;
};

