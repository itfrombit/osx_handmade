///////////////////////////////////////////////////////////////////////
// osx_handmade_playback.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//


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

		char Filename[FILENAME_MAX];
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

		char Filename[FILENAME_MAX];
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


