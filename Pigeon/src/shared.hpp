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

// TODO: Replace with template versions
#define Assert(x) {if (!(x)) *((u8*) 0) = 0;}
#define ArrayCount(x) sizeof(x) / sizeof(x[0])

// NOTE: Raise a compiler error when switching over an enum and any enum values are missing a case.
// https://msdn.microsoft.com/en-us/library/fdt9w8tf.aspx
#pragma warning (error: 4062)

// TODO: Replace
inline u32
StringCopy(c8* dst, c8* src)
{
	c8* start = dst;

	while (*src)
		*dst++ = *src++;

	*dst = '\0';

	return (u32) (dst - start);
}

struct v2i
{
	i32 x;
	i32 y;
};
