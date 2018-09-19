///////////////////////////////////////////////////////////////////////
// osx_handmade.h
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

#pragma once

#include <AudioUnit/AudioUnit.h>
#include <stdint.h>
#include <math.h>
#include <dispatch/dispatch.h>
#include <glob.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>


//#import <CoreVideo/CVDisplayLink.h>

#include <CoreGraphics/CGGeometry.h>
#include <CoreGraphics/CGEventSource.h>
#include <CoreGraphics/CGEventTypes.h>

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <IOKit/hid/IOHIDLib.h>


struct osx_offscreen_buffer
{
	// NOTE: Pixels are always 32-bits wide. BB GG RR XX
	void* Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};


struct osx_window_dimension
{
	int Width;
	int Height;
};


struct osx_game_code
{
	void* GameCodeDL;
	time_t DLLastWriteTime;

    // IMPORTANT(casey): Either of the callbacks can be 0!  You must
    // check before calling.
	game_update_and_render* UpdateAndRender;
    game_get_sound_samples* GetSoundSamples;
	debug_game_frame_end* DEBUGFrameEnd;

    bool32 IsValid;
};


struct osx_sound_output
{
	game_sound_output_buffer SoundBuffer;
	u32 SoundBufferSize;
	s16* CoreAudioBuffer;
	s16* ReadCursor;
	s16* WriteCursor;

	AudioStreamBasicDescription AudioDescriptor;
	AudioUnit AudioUnit;

#if 0
	AudioQueueRef AudioQueue;
	AudioQueueBufferRef AudioBuffers[2];
	bool32 RanAFrame;
#endif
};


void OSXInitCoreAudio(osx_sound_output* SoundOutput);
void OSXStopCoreAudio(osx_sound_output* SoundOutput);


struct osx_replay_buffer
{
	char Filename[FILENAME_MAX];
};

enum osx_memory_block_flag
{
	OSXMem_AllocatedDuringLooping = 0x1,
	OSXMem_FreedDuringLooping = 0x2
};

struct osx_memory_block
{
	platform_memory_block Block;

	osx_memory_block* Prev;
	osx_memory_block* Next;
	u64 LoopingFlags;
	u64 TotalAllocatedSize;

	u64 Pad[7];
};


struct osx_saved_memory_block
{
	u64 BasePointer;
	u64 Size;
};


struct osx_state
{
	ticket_mutex MemoryMutex;
	osx_memory_block MemorySentinel;

	int RecordingHandle;
	int InputRecordingIndex;

	int PlaybackHandle;
	int InputPlayingIndex;

	char AppFilename[FILENAME_MAX];
	char* OnePastLastAppFilenameSlash;
};


//extern osx_state GlobalOSXState;


#ifndef HANDMADE_USE_ASM_RDTSC
// NOTE(jeff): Thanks to @visitect for this suggestion
#define rdtsc __builtin_readcyclecounter
#else
internal inline uint64
rdtsc()
{
	u32 eax = 0;
	u32 edx;

	__asm__ __volatile__("cpuid;"
			     "rdtsc;"
				: "+a" (eax), "=d" (edx)
				:
				: "%rcx", "%rbx", "memory");

	__asm__ __volatile__("xorl %%eax, %%eax;"
			     "cpuid;"
				:
				:
				: "%rax", "%rbx", "%rcx", "%rdx", "memory");

	return (((u64)edx << 32) | eax);
}
#endif


#if HANDMADE_INTERNAL
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory);
DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile);
DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile);
DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(DEBUGExecuteSystemCommand);
DEBUG_PLATFORM_GET_PROCESS_STATE(DEBUGGetProcessState);
DEBUG_PLATFORM_GET_MEMORY_STATS(OSXGetMemoryStats);
#endif


struct osx_platform_file_group
{
	memory_arena Memory;
};


PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(OSXGetAllFilesOfTypeBegin);
PLATFORM_GET_ALL_FILE_OF_TYPE_END(OSXGetAllFilesOfTypeEnd);
PLATFORM_OPEN_FILE(OSXOpenFile);
PLATFORM_READ_DATA_FROM_FILE(OSXReadDataFromFile);
PLATFORM_FILE_ERROR(OSXFileError);

void OSXFreeMemoryBlock(osx_memory_block* Block);
PLATFORM_ALLOCATE_MEMORY(OSXAllocateMemory);
PLATFORM_DEALLOCATE_MEMORY(OSXDeallocateMemory);

void* OSXSimpleAllocateMemory(umm Size);


void OSXGetAppFilename(osx_state *State);

void OSXBuildAppPathFilename(osx_state *State, char *Filename,
                             int DestCount, char *Dest);

time_t OSXGetLastWriteTime(const char* Filename);
float OSXGetSecondsElapsed(u64 Then, u64 Now);

osx_game_code OSXLoadGameCode(const char* SourceDLName);
void OSXUnloadGameCode(osx_game_code* GameCode);


void OSXGetInputFileLocation(osx_state* State, bool32 InputStream, int SlotIndex, int DestCount, char* Dest);
void OSXBeginRecordingInput(osx_state* State, int InputRecordingIndex);
void OSXEndRecordingInput(osx_state* State);
void OSXBeginInputPlayback(osx_state* State, int InputPlayingIndex);
void OSXEndInputPlayback(osx_state* State);
void OSXRecordInput(osx_state* State, game_input* NewInput);
void OSXPlaybackInput(osx_state* State, game_input* NewInput);


struct platform_work_queue_entry
{
	platform_work_queue_callback* Callback;
	void* Data;
};


struct platform_work_queue
{
	u32 volatile CompletionGoal;
	u32 volatile CompletionCount;

	u32 volatile NextEntryToWrite;
	u32 volatile NextEntryToRead;
	dispatch_semaphore_t SemaphoreHandle;

	platform_work_queue_entry Entries[256];

	b32 NeedsOpenGL;
};


struct osx_thread_startup
{
    //int LogicalThreadIndex;
    //CGLContextObj OpenGLContext;
    platform_work_queue *Queue;
};


void OSXMakeQueue(platform_work_queue* Queue, uint32 ThreadCount);
void OSXAddEntry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data);
bool32 OSXDoNextWorkQueueEntry(platform_work_queue* Queue);
void OSXCompleteAllWork(platform_work_queue *Queue);



#define MAX_HID_BUTTONS 32

typedef struct osx_game_data
{
	// input
	IOHIDManagerRef				HIDManager;
	int							HIDX;
	int							HIDY;
	u8							HIDButtons[MAX_HID_BUTTONS];

	char SourceGameCodeDLFullPath[FILENAME_MAX];

	//umm							FrameTempArenaSize;
	//memory_arena				FrameTempArena;

	umm							CurrentSortMemorySize;
	void*						SortMemory;

	umm							CurrentClipMemorySize;
	void*						ClipMemory;

	u32							PushBufferSize;
	u8*							PushBuffer;

	u32							MaxVertexCount;
	textured_vertex*			VertexArray;
	renderer_texture*			BitmapArray;

	texture_op					TextureQueueMemory[512];
	//texture_op					TextureQueueMemory[256];
	u32							TextureOpCount;
	renderer_texture_queue		TextureQueue;
	u32							MaxQuadCountPerFrame;
	platform_renderer*			Renderer;


	game_offscreen_buffer		RenderBuffer;

	game_input					Input[2];
	game_input*					NewInput;
	game_input*					OldInput;

	//osx_state					OSXState;
	osx_game_code				Game;

	osx_sound_output			SoundOutput;

	osx_thread_startup			HighPriorityStartups[6];
	platform_work_queue			HighPriorityQueue;

	osx_thread_startup			LowPriorityStartups[2];
	platform_work_queue			LowPriorityQueue;

	//texture_op_queue			TextureOpQueue;

	u32							MonitorRefreshHz;
	f32							GameUpdateHz;

	u32							ExpectedFramesPerUpdate;

	u32							TargetFramesPerSecond;
	r32							TargetSecondsPerFrame;

	r64							MachTimebaseConversionFactor;
	int							SetupComplete;

	int							RenderAtHalfSpeed;

	u64							LastCounter;

	int							RenderCommandsInitialized;
	game_render_commands		RenderCommands;

	lighting_box*				LightBoxes;

	u8							KeyboardState[255];
	u8							OldKeyboardState[255];
} osx_game_data;


void OSXSetupGamepad(osx_game_data* game_data);

void OSXUpdateGlobalDebugTable();


void OSXToggleGlobalPause();

b32 OSXIsGameRunning();
void OSXStopGame();


inline void OSXProcessKeyboardMessage(game_button_state* NewState, b32 IsDown)
{
	if (NewState->EndedDown != IsDown)
	{
		NewState->EndedDown = IsDown;
		++NewState->HalfTransitionCount;
	}
}

void OSXKeyProcessing(b32 IsDown, u32 KeyCode, u32 Key,
					  int ShiftKeyFlag, int CommandKeyFlag, int ControlKeyFlag, int AlternateKeyFlag,
					  game_input* Input, osx_game_data* GameData);

inline b32x OSXIsInLoop(osx_state* State)
{
	b32x Result = (State->InputRecordingIndex || State->InputPlayingIndex);
	return Result;
}

#if HANDMADE_INTERNAL
#define OSXDebugLogOpenGLErrors(l) OSXDebugInternalLogOpenGLErrors(l)
#else
#define OSXDebugLogOpenGLErrors(l) {}
#endif

void OSXDebugInternalLogOpenGLErrors(const char* label);
void OSXSetupSound(osx_game_data* GameData);
void OSXSetupGameData(NSWindow* Window, osx_game_data* GameData);
void OSXInitOpenGL();
void OSXSetupGameRenderBuffer(osx_game_data* GameData, float Width, float Height, int BytesPerPixel);

void OSXInitializeGameInputForNewFrame(osx_game_data* GameData);

struct osx_mouse_data;

void OSXProcessFrameAndRunGameLogic(osx_game_data* GameData, CGRect WindowFrame, osx_mouse_data* MouseData);


