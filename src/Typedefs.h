#pragma once

#if defined(_MSC_VER) && defined(__cplusplus)
	#define STATIC_ASSERT static_assert
#else
	#define STATIC_ASSERT _Static_assert
#endif

#define DEBUG_BREAK() __asm__ volatile("int $0x03")
#define ASSERT(x) do { if (x) {} else { DEBUG_BREAK(); } } while (0)

#define cast(type) (type)

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

typedef signed char			s8;
typedef signed short		s16;
typedef signed int			s32;
typedef signed long long	s64;

typedef float				f32;
typedef double				f64;

enum Bool {
	false = 0,
	true = 1
};

typedef u8					b8;
typedef u32					b32;

#define SIZE_ASSERT(type, size) \
	STATIC_ASSERT(sizeof(type) == size, "Expected sizeof " #type " to be " #size " bytes.")

SIZE_ASSERT(u8,  1);
SIZE_ASSERT(u16, 2);
SIZE_ASSERT(u32, 4);
SIZE_ASSERT(u64, 8);

SIZE_ASSERT(s8,  1);
SIZE_ASSERT(s16, 2);
SIZE_ASSERT(s32, 4);
SIZE_ASSERT(s64, 8);

SIZE_ASSERT(f32, 4);
SIZE_ASSERT(f64, 8);

SIZE_ASSERT(b8,  1);
SIZE_ASSERT(b32, 4);

#undef SIZE_ASSERT
