///////////////////////////////////////////////////////////////////////
// osx_handmade_dylib.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

void OSXUnloadCode(osx_loaded_code* Loaded)
{
	if (Loaded->DL)
	{
		// TODO(casey): Currently, we never unload libraries, because
		// we may still be pointing to strings that are inside them
		// (despite our best efforts). Should we just make "never unload"
		// be the policy?
		//
		//dlclose(Loaded->DL);
		//
		Loaded->DL = 0;
	}

	Loaded->IsValid = false;
	ZeroArray(Loaded->FunctionCount, Loaded->Functions);
}


void OSXLoadCode(osx_state* State, osx_loaded_code* Loaded)
{
	char TempDLName[FILENAME_MAX];

	// Only do this if there is no lock file

	struct stat FileStat;
	int StatStatus = stat(Loaded->LockFullPath, &FileStat);

	if (StatStatus == -1)
	{
		Loaded->DLLastWriteTime = OSXGetLastWriteTime(Loaded->DLFullPath);

		for (u32 AttemptIndex = 0;
			 AttemptIndex < 128;
			 ++AttemptIndex)
		{
			OSXBuildAppPathFilename(State, Loaded->TransientDLName, ++Loaded->TempDLNumber,
									sizeof(TempDLName), TempDLName);

			if (Loaded->TempDLNumber >= 1024)
			{
				Loaded->TempDLNumber = 0;
			}

			if (OSXCopyFile(Loaded->DLFullPath, TempDLName) == 0)
			{
				break;
			}
		}

		Loaded->DL = dlopen(TempDLName, RTLD_LAZY|RTLD_GLOBAL);
		if (Loaded->DL)
		{
			printf("OSXLoadCode: loading %s successful\n", TempDLName);

			Loaded->IsValid = true;

			for (u32 FunctionIndex = 0;
				 FunctionIndex < Loaded->FunctionCount;
				 ++FunctionIndex)
			{
				void* Function = dlsym(Loaded->DL, Loaded->FunctionNames[FunctionIndex]);
				if (Function)
				{
					Loaded->Functions[FunctionIndex] = Function;
				}
				else
				{
					Loaded->IsValid = false;
				}
			}
		}
		else
		{
			printf("OSXLoadCode: error loading %s\n", TempDLName);
		}
	}

	if (!Loaded->IsValid)
	{
		OSXUnloadCode(Loaded);
	}
}


b32x OSXCheckForCodeChange(osx_loaded_code* Loaded)
{
	b32x Result = false;

	time_t NewDLWriteTime = OSXGetLastWriteTime(Loaded->DLFullPath);
	if (NewDLWriteTime != Loaded->DLLastWriteTime)
	{
		Result = true;
	}

	return Result;
}


void OSXReloadCode(osx_state* OSXState, osx_loaded_code* Loaded)
{
	OSXUnloadCode(Loaded);

	for (u32 LoadTryIndex = 0;
		 !Loaded->IsValid && (LoadTryIndex < 100);
		 ++LoadTryIndex)
	{
		OSXLoadCode(OSXState, Loaded);
		usleep(100);
	}
}

