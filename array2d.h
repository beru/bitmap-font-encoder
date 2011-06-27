#pragma once

#include <vector>

template <typename T>
class Array2D
{
public:
	Array2D()
		:
		values_(0)
	{
	}
	
	Array2D& operator = (const Array2D& a)
	{
		Resize(a.w_, a.h_);
		std::copy(a.values_, a.values_ + w_*h_, values_);
		return *this;
	}
	
	~Array2D()
	{
		delete values_;
	}

	void Resize(size_t w, size_t h)
	{
		if (values_) {
			delete[] values_;
		}
		values_ = new T[w*h];
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
	
private:
	size_t w_;
	size_t h_;
	T* values_;
};
