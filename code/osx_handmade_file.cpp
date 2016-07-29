///////////////////////////////////////////////////////////////////////
// osx_handmade_file.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//


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


void
OSXBuildAppPathFilename(osx_state *State, char *Filename,
                          int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastAppFilenameSlash - State->AppFilename, State->AppFilename,
               StringLength(Filename), Filename,
               DestCount, Dest);
}


#if HANDMADE_INTERNAL
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
#endif


//#define PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(name) platform_file_group *name(char *Type)
PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(OSXGetAllFilesOfTypeBegin)
{
	platform_file_group Result = {};

	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)mmap(0,
											  sizeof(osx_platform_file_group),
											  PROT_READ | PROT_WRITE,
											  MAP_PRIVATE | MAP_ANON,
											  -1,
											  0);
	Result.Platform = OSXFileGroup;

	const char* WildCard = "*.*";
	switch (Type)
	{
		case PlatformFileType_AssetFile:
			{
				WildCard = "*.hha";
			}
			break;

		case PlatformFileType_SavedGameFile:
			{
				WildCard = "*.hhs";
			}
			break;

		InvalidDefaultCase;
	}

	Result.FileCount = 0;

	if (glob(WildCard, 0, 0, &OSXFileGroup->GlobResults) == 0)
	{
		Result.FileCount = OSXFileGroup->GlobResults.gl_pathc;
		OSXFileGroup->CurrentIndex = 0;
	}

	return Result;
}


//#define PLATFORM_GET_ALL_FILE_OF_TYPE_END(name) void name(platform_file_group *FileGroup)
PLATFORM_GET_ALL_FILE_OF_TYPE_END(OSXGetAllFilesOfTypeEnd)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup->Platform;
	if (OSXFileGroup)
	{
		globfree(&OSXFileGroup->GlobResults);

		munmap(OSXFileGroup, sizeof(osx_platform_file_group));
	}
}


//#define PLATFORM_OPEN_FILE(name) platform_file_handle *name(platform_file_group *FileGroup)
PLATFORM_OPEN_FILE(OSXOpenNextFile)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup->Platform;
	platform_file_handle Result = {};

	if (OSXFileGroup->CurrentIndex < FileGroup->FileCount)
	{
		osx_platform_file_handle *OSXFileHandle = (osx_platform_file_handle*)mmap(0,
												 sizeof(osx_platform_file_handle),
												 PROT_READ | PROT_WRITE,
												 MAP_PRIVATE | MAP_ANON,
												 -1,
												 0);
		Result.Platform = OSXFileHandle;

		if (OSXFileHandle)
		{
			char* Filename = *(OSXFileGroup->GlobResults.gl_pathv + OSXFileGroup->CurrentIndex);

			int i = 0;
			while (i < sizeof(OSXFileHandle->Filename) && (Filename[i] != '\0'))
			{
				OSXFileHandle->Filename[i] = Filename[i];
				++i;
			}
			OSXFileHandle->Filename[i] = '\0';

			//strncpy(OSXFileHandle->Filename, Filename, sizeof(OSXFileHandle->Filename));
			OSXFileHandle->OSXFileHandle = open(Filename, O_RDONLY);
			Result.NoErrors = (OSXFileHandle->OSXFileHandle != -1);

			if (OSXFileHandle->OSXFileHandle != -1)
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

	return Result;
}


//#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Dest)
PLATFORM_READ_DATA_FROM_FILE(OSXReadDataFromFile)
{
	osx_platform_file_handle* OSXFileHandle = (osx_platform_file_handle*)Source->Platform;

	if (PlatformNoFileErrors(Source))
	{
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
			OSXFileError(Source, "Read file failed.");
		}
	}
	else
	{
		printf("OSXReadDataFromFile had pre-existing file errors on file: %s\n", OSXFileHandle->Filename);
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

