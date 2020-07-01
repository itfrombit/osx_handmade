

void* OSXSimpleAllocateMemory(umm Size)
{
	void* P = (void*)mmap(0, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

	if (P == MAP_FAILED)
	{
		printf("OSXAllocateMemory: mmap error: %d  %s", errno, strerror(errno));
	}

	Assert(P);

	return P;
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
	else if (Flags & PlatformMemory_OverflowCheck)
	{
		umm SizeRoundedUp = AlignPow2(Size, PageSize);
		TotalSize += SizeRoundedUp + 2 * PageSize;
		BaseOffset = PageSize + SizeRoundedUp - Size;
		ProtectOffset = PageSize + SizeRoundedUp;
	}

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
	}

	osx_memory_block* Sentinel = &GlobalOSXState.MemorySentinel;
	Block->Next = Sentinel;
	Block->Block.Size = Size;
	Block->TotalAllocatedSize = TotalSize;
	Block->Block.Flags = Flags;
	Block->LoopingFlags = 0;
	if (OSXIsInLoop(&GlobalOSXState) && !(Flags & PlatformMemory_NotRestored))
	{
		Block->LoopingFlags = OSXMem_AllocatedDuringLooping;
	}

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

		if (OSXIsInLoop(&GlobalOSXState) && !(OSXBlock->Block.Flags & PlatformMemory_NotRestored))
		{
			OSXBlock->LoopingFlags = OSXMem_FreedDuringLooping;
		}
		else
		{
			OSXFreeMemoryBlock(OSXBlock);
		}
	}
}

