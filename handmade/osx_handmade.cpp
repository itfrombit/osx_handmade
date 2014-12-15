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
#include <sys/stat.h>	// fstat()
#include <unistd.h>		// lseek()
#include <fcntl.h>

#include "osx_handmade.h"

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char* Filename)
{
	debug_read_file_result Result = {};

	int fd = open(Filename, O_RDONLY);
	if (fd != -1)
	{
		struct stat fileStat;
		if (fstat(fd, &fileStat) == 0)
		{
			uint32 FileSize32 = fileStat.st_size;
			Result.Contents = (char*)malloc(FileSize32);
			if (Result.Contents)
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
			}
		}
		else
		{
		}

		close(fd);
	}
	else
	{
	}

	return Result;
}


internal void
DEBUGPlatformFreeFileMemory(void* Memory)
{
	if (Memory)
	{
		free(Memory);
	}
}


internal bool32
DEBUGPlatformWriteEntireFile(char* Filename, uint32 MemorySize, void* Memory)
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



