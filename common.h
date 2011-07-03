#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "trace.h"

inline
size_t GetFileSize(FILE* file)
{
	fseek(file, 0, SEEK_END);
	int length = ftell(file);
	fseek(file, 0, SEEK_SET);
	return length;
}

