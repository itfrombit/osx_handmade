/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Jeff Buck $
   $Notice: (C) Copyright 2014. All Rights Reserved. $
   ======================================================================== */

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
#include <mach/vm_map.h>
#include <libkern/OSAtomic.h> // for OSMemoryBarrier
#include <pthread.h>

#ifdef HANDMADE_MIN_OSX
#include "handmade_platform.h"
#else
#include "../handmade/handmade_platform.h"
#endif

#include "osx_handmade.h"


// NOTE(jeff): vvvv Workaround hacks to avoid including handmade.h here.
// If we include it, we will get duplicate symbol linker errors
// for PushSize_().
#ifndef ArrayCount
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#endif

#ifndef Assert

#if HANDMADE_SLOW
// TODO(casey): Complete assertion macro - don't worry everyone!
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#endif
// NOTE(jeff): ^^^^ End of workaround hacks.


static void
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount, char *Dest)
{
    // TODO(casey): Dest bounds checking!
    
    for(int Index = 0;
        Index < SourceACount;
        ++Index)
    {
        *Dest++ = *SourceA++;
    }

    for(int Index = 0;
        Index < SourceBCount;
        ++Index)
    {
        *Dest++ = *SourceB++;
    }

    *Dest++ = 0;
}

void
OSXGetAppFilename(osx_state *State)
{
    // NOTE(casey): Never use MAX_PATH in code that is user-facing, because it
    // can be dangerous and lead to bad results.
    //
	pid_t PID = getpid();
	int r = proc_pidpath(PID, State->AppFilename, sizeof(State->AppFilename));

	if (r <= 0)
	{
		fprintf(stderr, "Error getting process path: pid %d: %s\n", PID, strerror(errno));
	}
	else
	{
		printf("process pid: %d   path: %s\n", PID, State->AppFilename);
	}

    State->OnePastLastAppFilenameSlash = State->AppFilename;
    for(char *Scan = State->AppFilename;
        *Scan;
        ++Scan)
    {
        if(*Scan == '/')
        {
            State->OnePastLastAppFilenameSlash = Scan + 1;
        }
    }
}

static int
StringLength(char *String)
{
    int Count = 0;
    while(*String++)
    {
        ++Count;
    }
    return(Count);
}

void
OSXBuildAppPathFilename(osx_state *State, char *Filename,
                          int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastAppFilenameSlash - State->AppFilename, State->AppFilename,
               StringLength(Filename), Filename,
               DestCount, Dest);
}



DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if (Memory)
	{
		free(Memory);
	}
}


DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};

	int fd = open(Filename, O_RDONLY);
	if (fd != -1)
	{
		struct stat fileStat;
		if (fstat(fd, &fileStat) == 0)
		{
			uint32 FileSize32 = fileStat.st_size;

			int result = -1;

#if 1
			Result.Contents = (char*)malloc(FileSize32);
			if (Result.Contents)
			{
				result = 0;
			}
#else	
			kern_return_t kresult = vm_allocate((vm_map_t)mach_task_self(),
									            (vm_address_t*)&Result.Contents,
									            FileSize32,
									            VM_FLAGS_ANYWHERE);

			if ((result == KERN_SUCCESS) && Result.Contents)
			{
				result = 0;
			}
#endif

			if (result == 0)
			{
				ssize_t BytesRead;
				BytesRead = read(fd, Result.Contents, FileSize32);
				if (BytesRead == FileSize32) // should have read until EOF
				{
					Result.ContentsSize = FileSize32;
				}
				else
				{
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				printf("DEBUGPlatformReadEntireFile %s:  vm_allocate error: %d: %s\n",
				       Filename, errno, strerror(errno));
			}
		}
		else
		{
			printf("DEBUGPlatformReadEntireFile %s:  fstat error: %d: %s\n",
			       Filename, errno, strerror(errno));
		}

		close(fd);
	}
	else
	{
		printf("DEBUGPlatformReadEntireFile %s:  open error: %d: %s\n",
		       Filename, errno, strerror(errno));
	}

	return Result;
}


DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool32 Result = false;

	int fd = open(Filename, O_WRONLY | O_CREAT, 0644);
	if (fd != -1)
	{
		ssize_t BytesWritten = write(fd, Memory, MemorySize);
		Result = (BytesWritten == MemorySize);

		if (!Result)
		{
			// TODO(jeff): Logging
		}

		close(fd);
	}
	else
	{
	}

	return Result;
}





//#define PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(name) platform_file_group *name(char *Type)
PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(OSXGetAllFilesOfTypeBegin)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)mmap(0,
											  sizeof(osx_platform_file_group),
											  PROT_READ | PROT_WRITE,
											  MAP_PRIVATE | MAP_ANON,
											  -1,
											  0);

	char* TypeAt = Type;
	char WildCard[32] = "*.";

	u32 WildCardIndex = 2;
	for (;
		 WildCardIndex < sizeof(WildCard);
		 ++WildCardIndex)
	{
		WildCard[WildCardIndex] = *TypeAt;
		if (*TypeAt == 0)
		{
			break;
		}

		++TypeAt;
	}

	WildCard[WildCardIndex] = '\0';

	OSXFileGroup->H.FileCount = 0;

	if (glob(WildCard, 0, 0, &OSXFileGroup->GlobResults) == 0)
	{
		OSXFileGroup->H.FileCount = OSXFileGroup->GlobResults.gl_pathc;
		OSXFileGroup->CurrentIndex = 0;
	}

	return (platform_file_group*)OSXFileGroup;
}


//#define PLATFORM_GET_ALL_FILE_OF_TYPE_END(name) void name(platform_file_group *FileGroup)
PLATFORM_GET_ALL_FILE_OF_TYPE_END(OSXGetAllFilesOfTypeEnd)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup;
	if (OSXFileGroup)
	{
		globfree(&OSXFileGroup->GlobResults);

		munmap(OSXFileGroup, sizeof(osx_platform_file_group));
	}
}


//#define PLATFORM_OPEN_FILE(name) platform_file_handle *name(platform_file_group *FileGroup)
PLATFORM_OPEN_FILE(OSXOpenNextFile)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup;
	osx_platform_file_handle* Result = 0;

	if (OSXFileGroup->CurrentIndex < OSXFileGroup->H.FileCount)
	{
		Result = (osx_platform_file_handle*)mmap(0,
												 sizeof(osx_platform_file_handle),
												 PROT_READ | PROT_WRITE,
												 MAP_PRIVATE | MAP_ANON,
												 -1,
												 0);

		if (Result)
		{
			char* Filename = *(OSXFileGroup->GlobResults.gl_pathv + OSXFileGroup->CurrentIndex);

			strcpy(Result->Filename, Filename);
			Result->OSXFileHandle = open(Filename, O_RDONLY);
			Result->H.NoErrors = (Result->OSXFileHandle != -1);

			if (Result->OSXFileHandle != -1)
			{
				printf("Loading asset %s\n", Filename);
			}
			else
			{
				printf("Error opening asset file %s\n", Filename);
			}

		}

		++OSXFileGroup->CurrentIndex;
	}
	else
	{
		printf("Ran out of assets to load.\n");
	}

	return (platform_file_handle*)Result;
}


//#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Dest)
PLATFORM_READ_DATA_FROM_FILE(OSXReadDataFromFile)
{
	if (PlatformNoFileErrors(Source))
	{
		osx_platform_file_handle* OSXFileHandle = (osx_platform_file_handle*)Source;

		// TODO(jeff): Consider mmap instead of open/read for overlapped IO.
		// TODO(jeff): If sticking with read, make sure to handle interrupted read.

		u64 BytesRead = pread(OSXFileHandle->OSXFileHandle, Dest, Size, Offset);

		if (BytesRead == Size)
		{
			// NOTE(jeff): File read succeeded
			//printf("Read file: %s read %ld bytes.\n", OSXFileHandle->Filename, BytesRead);
		}
		else
		{
			OSXFileError(&OSXFileHandle->H, "Read file failed.");
		}
	}
	else
	{
		osx_platform_file_handle* OSXFileHandle = (osx_platform_file_handle*)Source;
		printf("We got errors in OSXReadDataFromFile: %s\n", OSXFileHandle->Filename);
	}
}


//#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message)
PLATFORM_FILE_ERROR(OSXFileError)
{
#if HANDMADE_INTERNAL
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "OSX FILE ERROR: %s\n", Message);
#endif

	Handle->NoErrors = false;
}



time_t
OSXGetLastWriteTime(const char* Filename)
{
	time_t LastWriteTime = 0;

	struct stat FileStat;
	if (stat(Filename, &FileStat) == 0)
	{
		LastWriteTime = FileStat.st_mtimespec.tv_sec;
	}

#if 0
	int fd = open(Filename, O_RDONLY);
	if (fd != -1)
	{
		struct stat FileStat;
		if (fstat(fd, &FileStat) == 0)
		{
			LastWriteTime = FileStat.st_mtimespec.tv_sec;
		}

		close(fd);
	}
#endif

	return LastWriteTime;
}


osx_game_code
OSXLoadGameCode(const char* SourceDLName)
{
	osx_game_code Result = {};

	// TODO(casey): Need to get the proper path here!
	// TODO(casey): Automatic determination of when updates are necessary

	Result.DLLastWriteTime = OSXGetLastWriteTime(SourceDLName);

	Result.GameCodeDL = dlopen(SourceDLName, RTLD_LAZY|RTLD_GLOBAL);
	if (Result.GameCodeDL)
	{
		Result.UpdateAndRender = (game_update_and_render*)
			dlsym(Result.GameCodeDL, "GameUpdateAndRender");

		Result.GetSoundSamples = (game_get_sound_samples*)
			dlsym(Result.GameCodeDL, "GameGetSoundSamples");

		Result.IsValid = Result.UpdateAndRender && Result.GetSoundSamples;
	}

	if (!Result.IsValid)
	{
		Result.UpdateAndRender = 0;
		Result.GetSoundSamples = 0;
	}

	return Result;
}


void OSXUnloadGameCode(osx_game_code* GameCode)
{
	if (GameCode->GameCodeDL)
	{
		dlclose(GameCode->GameCodeDL);
		GameCode->GameCodeDL = 0;
	}

	GameCode->IsValid = false;
	GameCode->UpdateAndRender = 0;
	GameCode->GetSoundSamples = 0;
}


void OSXGetInputFileLocation(osx_state* State, bool32 InputStream, int SlotIndex, int DestCount, char* Dest)
{
	char Temp[64];
	sprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
	OSXBuildAppPathFilename(State, Temp, DestCount, Dest);
}


static osx_replay_buffer*
OSXGetReplayBuffer(osx_state* State, int unsigned Index)
{
	Assert(Index < ArrayCount(State->ReplayBuffers));
	osx_replay_buffer* Result = &State->ReplayBuffers[Index];

	return Result;
}


void OSXBeginRecordingInput(osx_state* State, int InputRecordingIndex)
{
	printf("beginning recording input\n");

	osx_replay_buffer* ReplayBuffer = OSXGetReplayBuffer(State, InputRecordingIndex);

	if (State->InputPlayingIndex == 1)
	{
		printf("...first stopping input playback\n");
		OSXEndInputPlayback(State);
	}

	if (ReplayBuffer->MemoryBlock)
	{
		State->InputRecordingIndex = InputRecordingIndex;

		char Filename[OSX_STATE_FILENAME_COUNT];
		OSXGetInputFileLocation(State, true, InputRecordingIndex, sizeof(Filename), Filename);
		State->RecordingHandle = open(Filename, O_WRONLY | O_CREAT, 0644);

#if 0
		uint32 BytesToWrite = State->TotalSize;
		Assert(State->TotalSize == BytesToWrite);

		if (State->RecordingHandle != -1)
		{
			ssize_t BytesWritten = write(State->RecordingHandle, State->GameMemoryBlock, BytesToWrite);
			bool Result = (BytesWritten == BytesToWrite);

			if (!Result)
			{
				// TODO(jeff): Logging
				printf("write error recording input: %d: %s\n", errno, strerror(errno));
			}
		}
#endif

		memcpy(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
	}
}


void OSXEndRecordingInput(osx_state* State)
{
	close(State->RecordingHandle);
	State->InputRecordingIndex = 0;
	printf("ended recording input\n");
}


void OSXBeginInputPlayback(osx_state* State, int InputPlayingIndex)
{
	printf("beginning input playback\n");

	osx_replay_buffer* ReplayBuffer = OSXGetReplayBuffer(State, InputPlayingIndex);
	if (ReplayBuffer->MemoryBlock)
	{
		State->InputPlayingIndex = InputPlayingIndex;

		char Filename[OSX_STATE_FILENAME_COUNT];
		OSXGetInputFileLocation(State, true, InputPlayingIndex, sizeof(Filename), Filename);
		State->PlaybackHandle = open(Filename, O_RDONLY);

#if 0
		uint32 BytesToRead = State->TotalSize;
		Assert(State->TotalSize == BytesToRead);

		if (State->PlaybackHandle != -1)
		{
			ssize_t BytesRead;

			BytesRead = read(State->PlaybackHandle, State->GameMemoryBlock, BytesToRead);

			if (BytesRead != BytesToRead)
			{
				printf("read error beginning input playback: %d: %s\n", errno, strerror(errno));
			}
		}
#endif

		memcpy(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
	}
}


void OSXEndInputPlayback(osx_state* State)
{
	close(State->PlaybackHandle);
	State->InputPlayingIndex = 0;

	printf("ended input playback\n");
}


void OSXRecordInput(osx_state* State, game_input* NewInput)
{
	uint32 BytesWritten = write(State->RecordingHandle, NewInput, sizeof(*NewInput));

	if (BytesWritten != sizeof(*NewInput))
	{
		printf("write error recording input: %d: %s\n", errno, strerror(errno));
	}
}


void OSXPlaybackInput(osx_state* State, game_input* NewInput)
{
	size_t BytesRead = read(State->PlaybackHandle, NewInput, sizeof(*NewInput));

	if (BytesRead == 0)
	{
		// NOTE(casey): We've hit the end of the stream, go back to the beginning
		int PlayingIndex = State->InputPlayingIndex;
		OSXEndInputPlayback(State);
		OSXBeginInputPlayback(State, PlayingIndex);

		BytesRead = read(State->PlaybackHandle, NewInput, sizeof(*NewInput));

		if (BytesRead != sizeof(*NewInput))
		{
			printf("read error rewinding playback input: %d: %s\n", errno, strerror(errno));
		}
		else
		{
			printf("rewinding playback...\n");
		}
	}
}


void* OSXQueueThreadProc(void *data)
{
	platform_work_queue* Queue = (platform_work_queue*)data;

	for(;;)
	{
		if(OSXDoNextWorkQueueEntry(Queue))
		{
			dispatch_semaphore_wait(Queue->SemaphoreHandle, DISPATCH_TIME_FOREVER);
		}
	}

	return(0);
}



void OSXMakeQueue(platform_work_queue* Queue, uint32 ThreadCount)
{
	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0;

	Queue->NextEntryToWrite = 0;
	Queue->NextEntryToRead = 0;

	Queue->SemaphoreHandle = dispatch_semaphore_create(0);

	for (uint32 ThreadIndex = 0;
		 ThreadIndex < ThreadCount;
		 ++ThreadIndex)
	{
		pthread_t		ThreadId;

		int r = pthread_create(&ThreadId, NULL, OSXQueueThreadProc, Queue);
		if (r != 0)
		{
			printf("Error creating thread %d\n", ThreadIndex);
		}
	}
}


void OSXAddEntry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data)
{
    // TODO(casey): Switch to InterlockedCompareExchange eventually
    // so that any thread can add?
    uint32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
    platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;
    ++Queue->CompletionGoal;
    OSMemoryBarrier();
    // Not needed: _mm_sfence();
    Queue->NextEntryToWrite = NewNextEntryToWrite;
	dispatch_semaphore_signal(Queue->SemaphoreHandle);

#if 0
	int r = dispatch_semaphore_signal(Queue->SemaphoreHandle);
	if (r > 0)
	{
		printf("  jsb: A thread was woken\n");
	}
	else
	{
		printf("  jsb: No thread was woken\n");
	}
#endif
}

bool32 OSXDoNextWorkQueueEntry(platform_work_queue* Queue)
{
	bool32 WeShouldSleep = false;

	uint32 OriginalNextEntryToRead = Queue->NextEntryToRead;
	uint32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);

	if(OriginalNextEntryToRead != Queue->NextEntryToWrite)
	{
        // NOTE(jeff): OSAtomicCompareAndSwapXXX functions return 1 if the swap took place, 0 otherwise!
		uint32 SwapOccurred = OSAtomicCompareAndSwapIntBarrier(OriginalNextEntryToRead,
															   NewNextEntryToRead,
															   (int volatile*)&Queue->NextEntryToRead);

		if (SwapOccurred)
		{
			platform_work_queue_entry Entry = Queue->Entries[OriginalNextEntryToRead];
			Entry.Callback(Queue, Entry.Data);
			//InterlockedIncrement((int volatile *)&Queue->CompletionCount);
			OSAtomicIncrement32Barrier((int volatile *)&Queue->CompletionCount);
		}
		else
		{
		}
	}
	else
	{
		WeShouldSleep = true;
	}

	return(WeShouldSleep);
}

void OSXCompleteAllWork(platform_work_queue *Queue)
{
	while (Queue->CompletionGoal != Queue->CompletionCount)
	{
		OSXDoNextWorkQueueEntry(Queue);
	}

	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0;
}



