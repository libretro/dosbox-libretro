#pragma once
#include <cstdarg>
#include <cstddef>

//android libc is defective(printfs drop non ascii chars)
//this causes strings to cutoff at dos escape codes
int portable_vsnprintf(char *str, size_t str_m, const char *fmt, va_list ap);
