#include <stdarg.h>
int _snprintf_s(char* buffer, size_t offset, size_t junk, char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int n = vsprintf(buffer, fmt, args);
	va_end(args);
	return n;
}

int _snprintf_s(char* buffer, size_t offset, char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int n = vsprintf(buffer, fmt, args);
	va_end(args);
	return n;
}

