///////////////////////////////////////////////////////////////////////
// osx_main.mm
//
// Jeff Buck
// Copyright 2014-2017. All Rights Reserved.
//

/*
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
#include <mach-o/dyld.h>
#include <libkern/OSAtomic.h> // for OSMemoryBarrier
#include <pthread.h>

#define GL_GLEXT_LEGACY

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
//#include <OpenGL/glext.h>
//#include <OpenGL/glu.h>

#include "handmade_platform.h"


#define GL_BGRA_EXT		0x80E1

typedef void gl_bind_framebuffer(GLenum target, GLuint framebuffer);
typedef void gl_gen_framebuffers(GLsizei n, GLuint *framebuffers);
typedef void gl_framebuffer_texture_2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum gl_check_framebuffer_status(GLenum target);

global_variable gl_bind_framebuffer* glBindFramebuffer;
global_variable gl_gen_framebuffers* glGenFramebuffers;
global_variable gl_framebuffer_texture_2D* glFramebufferTexture2D;
global_variable gl_check_framebuffer_status* glCheckFramebufferStatus;




//#include "handmade_intrinsics.h"
//#include "handmade_math.h"
#include "handmade_shared.h"


#include "osx_handmade.h"

platform_api Platform;


global_variable b32 GlobalSoftwareRendering;
global_variable b32 GlobalRunning = 1;
global_variable b32 GlobalPause;
global_variable b32 GlobalShowSortGroups;

global_variable GLuint OpenGLDefaultInternalTextureFormat;



// NOTE(jeff): These are already defined in glext.h.
#undef GL_EXT_texture_sRGB
#undef GL_EXT_framebuffer_sRGB

#include "handmade_render.h"
#include "handmade_sort.cpp"
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

inline b32x OSXIsInLoop(osx_state* State)
{
	b32x Result = (State->InputRecordingIndex || State->InputPlayingIndex);
	return Result;
}

//#define PLATFORM_ALLOCATE_MEMORY(name) platform_memory_block* name(memory_index Size, u64 Flags)
PLATFORM_ALLOCATE_MEMORY(OSXAllocateMemory)
{
	// NOTE(casey): We require memory block headers not to change the cache
	// line alignment of an allocation
	Assert(sizeof(osx_memory_block) == 128);

	umm PageSize = 4096;
	umm TotalSize = Size + sizeof(osx_memory_block);
	umm BaseOffset = sizeof(osx_memory_block);
	umm ProtectOffset = 0;

	if (Flags & PlatformMemory_UnderflowCheck)
	{
		TotalSize += Size + 2 * PageSize;
		BaseOffset = 2 * PageSize;
		ProtectOffset = PageSize;
	}

	if (Flags & PlatformMemory_OverflowCheck)
	{
		umm SizeRoundedUp = AlignPow2(Size, PageSize);
		TotalSize += SizeRoundedUp + 2 * PageSize;
		BaseOffset = PageSize + SizeRoundedUp - Size;
		ProtectOffset = PageSize + SizeRoundedUp;
	}

	//osx_memory_block* Block = (osx_memory_block*)malloc(TotalSize);
	osx_memory_block* Block = (osx_memory_block*)mmap(0,
									TotalSize,
									PROT_READ | PROT_WRITE,
									MAP_PRIVATE | MAP_ANON,
									-1,
									0);
	if (Block == MAP_FAILED)
	{
		printf("OSXAllocateMemory: mmap error: %d  %s", errno, strerror(errno));
	}

	Assert(Block);
	Block->Block.Base = (u8*)Block + BaseOffset;
	Assert(Block->Block.Used == 0);
	Assert(Block->Block.ArenaPrev == 0);

	if (Flags & (PlatformMemory_UnderflowCheck|PlatformMemory_OverflowCheck))
	{
		int ProtectResult = mprotect((u8*)Block + ProtectOffset, PageSize, PROT_NONE);
		if (ProtectResult != 0)
		{
		}
		else
		{
			printf("OSXAllocateMemory: Underflow mprotect error: %d  %s", errno, strerror(errno));
		}
		//Block = (osx_memory_block*)((u8*)Block + PageSize);
	}

	osx_memory_block* Sentinel = &GlobalOSXState.MemorySentinel;
	Block->Next = Sentinel;
	Block->Block.Size = Size;
	Block->TotalAllocatedSize = TotalSize;
	Block->Block.Flags = Flags;
	Block->LoopingFlags = OSXIsInLoop(&GlobalOSXState) ? OSXMem_AllocatedDuringLooping : 0;

	BeginTicketMutex(&GlobalOSXState.MemoryMutex);
	Block->Prev = Sentinel->Prev;
	Block->Prev->Next = Block;
	Block->Next->Prev = Block;
	EndTicketMutex(&GlobalOSXState.MemoryMutex);

	platform_memory_block* PlatBlock = &Block->Block;
	return PlatBlock;
}


void OSXFreeMemoryBlock(osx_memory_block* Block)
{
	BeginTicketMutex(&GlobalOSXState.MemoryMutex);
	Block->Prev->Next = Block->Next;
	Block->Next->Prev = Block->Prev;
	EndTicketMutex(&GlobalOSXState.MemoryMutex);

	if (munmap(Block, Block->TotalAllocatedSize) != 0)
	{
		printf("OSXFreeMemoryBlock: munmap error: %d  %s", errno, strerror(errno));
	}
}

//#define PLATFORM_DEALLOCATE_MEMORY(name) void name(platform_memory_block* Memory)
PLATFORM_DEALLOCATE_MEMORY(OSXDeallocateMemory)
{
	if (Block)
	{
		osx_memory_block* OSXBlock = (osx_memory_block*)Block;

		if (OSXIsInLoop(&GlobalOSXState))
		{
			OSXBlock->LoopingFlags = OSXMem_FreedDuringLooping;
		}
		else
		{
			OSXFreeMemoryBlock(OSXBlock);
		}
	}
}


