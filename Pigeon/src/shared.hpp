#include <cstdint>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef char    c8;
typedef wchar_t c16;

typedef int b32;

typedef size_t index;

#define ArrayCount(x) sizeof(x) / sizeof(x[0])

#include <cstdio>
inline void
DebugPrint(c16 const* format, ...)
{
	int result;

	va_list args;
	va_start (args, format);
	c16 buffer[128];
	result = _vsnwprintf_s(buffer, ArrayCount(buffer), _TRUNCATE, format, args);
	OutputDebugStringW(buffer);
	va_end (args);
}