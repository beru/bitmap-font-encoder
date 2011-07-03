#pragma once

#if defined(_DEBUG) || defined(DEBUG)

class DbgStr
{
public:
	void operator() (const char* pszFormat, ...);
	void operator() (const wchar_t* pszFormat, ...);
};

#define TRACE     DbgStr()

#else
#define TRACE

#endif

