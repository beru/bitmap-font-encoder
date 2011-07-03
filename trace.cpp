#include "trace.h"

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

void DbgStr::operator() ( const char* pszFormat, ...)
{
	va_list	argp;
	static char pszBuf[10240];
	va_start(argp, pszFormat);
	vsprintf( pszBuf, pszFormat, argp);
	va_end(argp);
	OutputDebugStringA( pszBuf);
}

void DbgStr::operator() ( const wchar_t* pszFormat, ...)
{
	va_list	argp;
	static wchar_t pszBuf[10240];
	va_start(argp, pszFormat);
	_vswprintf( pszBuf, pszFormat, argp);
	va_end(argp);
	OutputDebugStringW( pszBuf);
}

