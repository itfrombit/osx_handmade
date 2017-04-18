///////////////////////////////////////////////////////////////////////
// osx_handmade_playback.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

extern osx_state GlobalOSXState;

void OSXGetInputFileLocation(osx_state* State, bool32 InputStream, int SlotIndex, int DestCount, char* Dest)
{
	char Temp[64];
	sprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
	OSXBuildAppPathFilename(State, Temp, DestCount, Dest);
}

#if 0
static osx_replay_buffer*
OSXGetReplayBuffer(osx_state* State, int unsigned Index)
{
	Assert(Index < ArrayCount(State->ReplayBuffers));
	osx_replay_buffer* Result = &State->ReplayBuffers[Index];

	return Result;
}
#endif


void OSXBeginRecordingInput(osx_state* State, int InputRecordingIndex)
{
	printf("beginning recording input\n");

	char Filename[FILENAME_MAX];
	OSXGetInputFileLocation(State, true, InputRecordingIndex,
							sizeof(Filename), Filename);
	State->RecordingHandle = open(Filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (State->RecordingHandle != -1)
	{
		// Test case: OSXVerifyMemoryListIntegrity();

		State->InputRecordingIndex = InputRecordingIndex;
		osx_memory_block* Sentinel = &GlobalOSXState.MemorySentinel;

		BeginTicketMutex(&GlobalOSXState.MemoryMutex);

		for (osx_memory_block* SourceBlock = Sentinel->Next;
				SourceBlock != Sentinel;
				SourceBlock = SourceBlock->Next)
		{
			if (!(SourceBlock->Block.Flags & PlatformMemory_NotRestored))
			{
				osx_saved_memory_block DestBlock;
				void* BasePointer = SourceBlock->Block.Base;
				DestBlock.BasePointer = (u64)BasePointer;
				DestBlock.Size = SourceBlock->Block.Size;

				ssize_t BytesWritten;
				BytesWritten = write(State->RecordingHandle, &DestBlock, sizeof(DestBlock));

				if (BytesWritten == sizeof(DestBlock))
				{
					Assert(DestBlock.Size <= U32Max);
					BytesWritten = write(State->RecordingHandle, BasePointer,
										(u32)DestBlock.Size);

					if (BytesWritten != DestBlock.Size)
					{
						printf("OSXBeginRecordingInput: Could not write Block contents\n");
					}
				}
				else
				{
					printf("OSXBeginRecordingInput: Could not write Block header\n");
				}
			}
		}

		EndTicketMutex(&GlobalOSXState.MemoryMutex);

		osx_saved_memory_block DestBlock = {};
		write(State->RecordingHandle, &DestBlock, sizeof(DestBlock));
	}
}


void OSXEndRecordingInput(osx_state* State)
{
	close(State->RecordingHandle);
	State->RecordingHandle = -1;
	State->InputRecordingIndex = 0;
	printf("ended recording input\n");
}

void OSXClearBlocksByMask(osx_state* State, u64 Mask)
{
	for (osx_memory_block* BlockIter = State->MemorySentinel.Next;
			BlockIter != &State->MemorySentinel;
			)
	{
		osx_memory_block* Block = BlockIter;
		BlockIter = BlockIter->Next;

		if ((Block->LoopingFlags & Mask) == Mask)
		{
			OSXFreeMemoryBlock(Block);
		}
		else
		{
			Block->LoopingFlags = 0;
		}
	}
}

void OSXBeginInputPlayback(osx_state* State, int InputPlayingIndex)
{
	OSXClearBlocksByMask(State, OSXMem_AllocatedDuringLooping);

	printf("beginning input playback\n");
	char Filename[FILENAME_MAX];
	OSXGetInputFileLocation(State, true, InputPlayingIndex, sizeof(Filename), Filename);
	State->PlaybackHandle = open(Filename, O_RDONLY);

	if (State->PlaybackHandle != -1)
	{
		State->InputPlayingIndex = InputPlayingIndex;

		for (;;)
		{
			osx_saved_memory_block Block = {};

			ssize_t BytesRead;
			BytesRead = read(State->PlaybackHandle, &Block, sizeof(Block));

			if (Block.BasePointer != 0)
			{
				void* BasePointer = (void*)Block.BasePointer;
				Assert(Block.Size <= U32Max);
				BytesRead = read(State->PlaybackHandle, BasePointer, (u32)Block.Size);
			}
			else
			{
				break;
			}
		}
	}
	else
	{
		printf("Could not open playback file %s\n", Filename);
	}
}


void OSXEndInputPlayback(osx_state* State)
{
	OSXClearBlocksByMask(State, OSXMem_FreedDuringLooping);

	close(State->PlaybackHandle);
	State->PlaybackHandle = -1;
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


