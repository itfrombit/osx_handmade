///////////////////////////////////////////////////////////////////////
// osx_handmade_file.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//
#include <copyfile.h>


int
OSXCopyFile(const char* From, const char* To)
{
	copyfile_state_t s = copyfile_state_alloc();
	int Result = copyfile(From, To, s, COPYFILE_ALL);
	copyfile_state_free(s);

	return Result;
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
OSXBuildAppPathFilename(osx_state *State, const char *Filename, u32 Unique,
                        int DestCount, char *Dest)
{
	string A =
	{
		(umm)(State->OnePastLastAppFilenameSlash - State->AppFilename),
		(u8*)State->AppFilename
	};

	string B = WrapZ((char*)Filename);

	if (Unique == 0)
	{
		FormatString(DestCount, Dest, "%S%S", A, B);
	}
	else
	{
		FormatString(DestCount, Dest, "%S%d_%S", A, Unique, B);
	}
}


void
OSXBuildAppPathFilename(osx_state *State, const char *Filename,
                        int DestCount, char *Dest)
{
	OSXBuildAppPathFilename(State, Filename, 0, DestCount, Dest);
}


#if HANDMADE_INTERNAL

#if 0
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

#endif

#if 0
internal platform_file_info*
OSXAllocateFileInfo(platform_file_group* FileGroup, struct stat* FileStat)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup->Platform;

	platform_file_info* Info = PushStruct(&OSXFileGroup->Memory, platform_file_info);
	Info->Next = FileGroup->FirstFileInfo;
	Info->FileDate = FileStat->st_mtimespec.tv_sec;
	Info->FileSize = FileStat->st_size;
	FileGroup->FirstFileInfo = Info;
	++FileGroup->FileCount;

	return Info;
}
#endif

PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(OSXGetAllFilesOfTypeBegin)
{
	platform_file_group Result = {};

	osx_platform_file_group* OSXFileGroup = BootstrapPushStruct(osx_platform_file_group, Memory);
	Result.Platform = OSXFileGroup;

	const char* Stem = "";
	const char* WildCard = "*.*";

	switch (Type)
	{
		case PlatformFileType_AssetFile:
		{
			Stem = "data/";
			WildCard = "data/*.hha";
		} break;

		case PlatformFileType_SavedGameFile:
		{
			Stem = "data/";
			WildCard = "data/*.hhs";
		} break;

		case PlatformFileType_HHT:
		{
			Stem = "tags/";
			WildCard = "tags/*.hht";
		} break;

		InvalidDefaultCase;
	}

	u32 StemSize = 0;
	for (const char* Scan = Stem; *Scan; ++Scan)
	{
		++StemSize;
	}

	glob_t GlobResults;

	if (glob(WildCard, 0, 0, &GlobResults) == 0)
	{
		Result.FileCount = GlobResults.gl_pathc;

		for (u32 CurrentIndex = 0; CurrentIndex < Result.FileCount; ++CurrentIndex)
		{
			const char* GlobFilename = *(GlobResults.gl_pathv + CurrentIndex);

			struct stat FileStat;
			int StatStatus = stat(GlobFilename, &FileStat);
			assert(StatStatus == 0);

			platform_file_info* Info = PushStruct(&OSXFileGroup->Memory, platform_file_info);

			Info->Next = Result.FirstFileInfo;
			Info->FileDate = FileStat.st_mtimespec.tv_sec;
			Info->FileSize = FileStat.st_size;

			const char* BaseNameBegin = GlobFilename;
			const char* BaseNameEnd = 0;
			const char* Scan = BaseNameBegin;

			while (*Scan)
			{
				if (Scan[0] == '/')
				{
					BaseNameBegin = Scan + 1;
				}

				if (Scan[0] == '.')
				{
					BaseNameEnd = Scan;
				}

				++Scan;
			}

			if (!BaseNameEnd)
			{
				BaseNameEnd = Scan;
			}

			u32 BaseNameSize = (u32)(BaseNameEnd - BaseNameBegin);
			Info->BaseName = PushAndNullTerminate(&OSXFileGroup->Memory, BaseNameSize, (char*)BaseNameBegin);

			for (char* Lower = Info->BaseName; *Lower; ++Lower)
			{
				*Lower = ToLowercase(*Lower);
			}

			Info->Platform = PushStringZ(&OSXFileGroup->Memory, (char*)GlobFilename);

			Result.FirstFileInfo = Info;
		}
	}

	globfree(&GlobResults);

	return Result;
}


PLATFORM_GET_ALL_FILE_OF_TYPE_END(OSXGetAllFilesOfTypeEnd)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup->Platform;

	if (OSXFileGroup)
	{
		Clear(&OSXFileGroup->Memory);
	}
}


internal PLATFORM_GET_FILE_BY_PATH(OSXGetFileByPath)
{
	osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup->Platform;
	platform_file_info* Result = 0;

	struct stat FileStat;
	int StatStatus = stat(Path, &FileStat);

	if ((StatStatus == 0) || (ModeFlags & OpenFile_Write))
	{
		Result = PushStruct(&OSXFileGroup->Memory, platform_file_info);
		Result->Next = FileGroup->FirstFileInfo;
		Result->FileDate = FileStat.st_mtimespec.tv_sec;
		Result->FileSize = FileStat.st_size;
		FileGroup->FirstFileInfo = Result;
		++FileGroup->FileCount;

		Result->BaseName = Path;
		Result->Platform = Path;
	}

	return Result;
}


PLATFORM_OPEN_FILE(OSXOpenFile)
{
	//osx_platform_file_group* OSXFileGroup = (osx_platform_file_group*)FileGroup->Platform;
	platform_file_handle Result = {};

	int FileOpenFlags = 0;

	if (ModeFlags & OpenFile_Write)
	{
		FileOpenFlags |= O_RDWR;
		FileOpenFlags |= O_CREAT;
	}
	else
	{
		FileOpenFlags |= O_RDONLY;
	}

	const char* Filename = (const char*)Info->Platform;

	int OSXFileHandle = open(Filename, FileOpenFlags);
	Result.NoErrors = (OSXFileHandle != -1);
	*(int*)&Result.Platform = OSXFileHandle;

	if (OSXFileHandle != -1)
	{
		printf("Loading asset %s\n", Filename);
	}
	else
	{
		printf("Error opening asset file %s\n", Filename);
	}

	return Result;
}


PLATFORM_READ_DATA_FROM_FILE(OSXReadDataFromFile)
{
	if (PlatformNoFileErrors(Handle))
	{
		int OSXFileHandle = *(int*)&Handle->Platform;

		// TODO(jeff): Consider mmap instead of open/read for overlapped IO.
		// TODO(jeff): If sticking with read, make sure to handle interrupted read.

		u64 BytesRead = pread(OSXFileHandle, Dest, Size, Offset);

		if (BytesRead == Size)
		{
			// NOTE(jeff): File read succeeded
			//printf("Read file: %s read %ld bytes.\n", OSXFileHandle->Filename, BytesRead);
		}
		else
		{
			OSXFileError(Handle, "Read file failed.");
		}
	}
	else
	{
		printf("OSXReadDataFromFile had pre-existing file errors\n");
	}
}


PLATFORM_WRITE_DATA_TO_FILE(OSXWriteDataToFile)
{
	if (PlatformNoFileErrors(Handle))
	{
		int OSXFileHandle = *(int*)&Handle->Platform;

		u64 BytesWritten = pwrite(OSXFileHandle, Source, Size, Offset);

		if (BytesWritten == Size)
		{
			// NOTE(jeff): File read succeeded
			//printf("Wrote file: %s wrote %ld bytes.\n", OSXFileHandle->Filename, BytesWritten);
		}
		else
		{
			OSXFileError(Handle, "Write file failed.");
		}
	}
	else
	{
		printf("OSXWriteDataToFile had pre-existing file errors\n");
	}
}


PLATFORM_CLOSE_FILE(OSXCloseFile)
{
	int OSXFileHandle = *(int*)&Handle->Platform;

	if (OSXFileHandle != -1)
	{
		close(OSXFileHandle);
	}
}


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

	return LastWriteTime;
}

