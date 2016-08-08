///////////////////////////////////////////////////////////////////////
// osx_main.mm
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

/*
	TODO(jeff): THIS IS NOT A FINAL PLATFORM LAYER!!!

	This will be updated to keep parity with Casey's win32 platform layer.
	See his win32_handmade.cpp file for TODO details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libproc.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/vm_map.h>
#include <libkern/OSAtomic.h> // for OSMemoryBarrier
#include <pthread.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>


#include "handmade_platform.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"


#include "osx_handmade.h"

global_variable bool32 GlobalPause;
global_variable b32 GlobalRunning = 1;
global_variable b32 GlobalUseSoftwareRendering;
global_variable GLuint OpenGLDefaultInternalTextureFormat;

global_variable platform_api Platform;

#include "handmade_opengl.cpp"
#include "handmade_render.cpp"

#include "osx_handmade_debug.cpp"
#include "osx_handmade_file.cpp"
#include "osx_handmade_process.cpp"
#include "osx_handmade_thread.cpp"
#include "osx_handmade_audio.cpp"
#include "osx_handmade_hid.cpp"
#include "osx_handmade_dylib.cpp"
#include "osx_handmade_playback.cpp"
#include "osx_handmade_game.cpp"


#if 0
// NOTE(jeff): vvvv Workaround hacks to avoid including handmade.h here.
// If we include it, we will get duplicate symbol linker errors
// for PushSize_().
#ifndef ArrayCount
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#endif
// NOTE(jeff): ^^^^ End of workaround hacks.
#endif


//#define PLATFORM_ALLOCATE_MEMORY(name) void *name(memory_index Size)
PLATFORM_ALLOCATE_MEMORY(OSXAllocateMemory)
{
	void* Result = malloc(Size);

	return(Result);
}


//#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void *Memory)
PLATFORM_DEALLOCATE_MEMORY(OSXDeallocateMemory)
{
	if (Memory)
	{
		free(Memory);
	}
}


