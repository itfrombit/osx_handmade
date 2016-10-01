///////////////////////////////////////////////////////////////////////
// osx_handmade_thread.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//


void* OSXQueueThreadProc(void *data)
{
	//platform_work_queue* Queue = (platform_work_queue*)data;
	osx_thread_startup* Thread = (osx_thread_startup*)data;
	platform_work_queue* Queue = Thread->Queue;

	for(;;)
	{
		if(OSXDoNextWorkQueueEntry(Queue))
		{
			dispatch_semaphore_wait(Queue->SemaphoreHandle, DISPATCH_TIME_FOREVER);
		}
	}

	return(0);
}



void OSXMakeQueue(platform_work_queue* Queue, uint32 ThreadCount, osx_thread_startup* Startups)
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
		osx_thread_startup* Startup = Startups + ThreadIndex;
		Startup->Queue = Queue;

		pthread_t		ThreadId;
		int r = pthread_create(&ThreadId, NULL, OSXQueueThreadProc, Startup);
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
		printf("  dispatch_semaphore_signal: A thread was woken\n");
	}
	else
	{
		printf("  dispatch_semaphore_signal: No thread was woken\n");
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

