///////////////////////////////////////////////////////////////////////
// osx_handmade_debug.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

#ifndef Assert

#if HANDMADE_SLOW
// TODO(casey): Complete assertion macro - don't worry everyone!
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#endif


float OSXGetSecondsElapsed(u64 Then, u64 Now)
{
	static mach_timebase_info_data_t tb;

	u64 Elapsed = Now - Then;

	if (tb.denom == 0)
	{
		// First time we need to get the timebase
		mach_timebase_info(&tb);
	}

	u64 Nanos = Elapsed * tb.numer / tb.denom;
	float Result = (float)Nanos * 1.0E-9;

	return Result;
}

void OSXVerifyMemoryListIntegrity()
{
	BeginTicketMutex(&GlobalOSXState.MemoryMutex);
	local_persist u32 FailCounter;

	osx_memory_block* Sentinel = &GlobalOSXState.MemorySentinel;

	for (osx_memory_block* SourceBlock = Sentinel->Next;
			SourceBlock != Sentinel;
			SourceBlock = SourceBlock->Next)
	{
		Assert(SourceBlock->Block.Size <= U32Max);
	}

	++FailCounter;

	EndTicketMutex(&GlobalOSXState.MemoryMutex);
}


#if HANDMADE_INTERNAL
DEBUG_PLATFORM_GET_MEMORY_STATS(OSXGetMemoryStats)
{
	debug_platform_memory_stats Stats = {};

	BeginTicketMutex(&GlobalOSXState.MemoryMutex);
	osx_memory_block* Sentinel = &GlobalOSXState.MemorySentinel;

	for (osx_memory_block* SourceBlock = Sentinel->Next;
			SourceBlock != Sentinel;
			SourceBlock = SourceBlock->Next)
	{
		Assert(SourceBlock->Block.Size <= U32Max);

		++Stats.BlockCount;
		Stats.TotalSize += SourceBlock->Block.Size;
		Stats.TotalUsed += SourceBlock->Block.Used;
	}
	EndTicketMutex(&GlobalOSXState.MemoryMutex);

	return Stats;
}
#endif



