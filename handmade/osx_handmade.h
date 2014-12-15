#if !defined(OSX_HANDMADE_H)

#include <stdint.h>
#include <math.h>


// NOTE(jeff): These typedefs are the same as in win32
#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;


struct osx_offscreen_buffer
{
	// NOTE: Pixels are always 32-bits wide. BB GG RR XX
	//BITMAPINFO Info;
	void* Memory;
	int Width;
	int Height;
	int Pitch;
};


struct osx_window_dimension
{
	int Width;
	int Height;
};


struct osx_sound_output
{
	int SamplesPerSecond;
	uint32 RunningSampleIndex;
	int BytesPerSample;
	real32 tSine;
	int LatencySampleCount;
};


#define OSX_HANDMADE_H
#endif
