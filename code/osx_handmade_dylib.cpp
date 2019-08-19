///////////////////////////////////////////////////////////////////////
// osx_handmade_dylib.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//


osx_game_code OSXLoadGameCode(osx_state* State, const char* SourceDLName)
{
	osx_game_code Result = {};

	char TempDLName[FILENAME_MAX];

	Result.DLLastWriteTime = OSXGetLastWriteTime(SourceDLName);

	while (State->TempDLNumber < 1024)
	{
		OSXBuildAppPathFilename(State, "libhandmade_temp.dylib", ++State->TempDLNumber,
				                sizeof(TempDLName), TempDLName);

		if (OSXCopyFile(SourceDLName, TempDLName) == 0)
		{
			break;
		}
		else
		{
			//++State->TempDLNumber;
		}
	}

	Result.GameCodeDL = dlopen(TempDLName, RTLD_LAZY|RTLD_GLOBAL);
	if (Result.GameCodeDL)
	{
		printf("OSXLoadGameCode: loading %s successful\n", TempDLName);

		Result.UpdateAndRender = (game_update_and_render*)
			dlsym(Result.GameCodeDL, "GameUpdateAndRender");

		Result.GetSoundSamples = (game_get_sound_samples*)
			dlsym(Result.GameCodeDL, "GameGetSoundSamples");

		Result.DEBUGFrameEnd = (debug_game_frame_end*)
			dlsym(Result.GameCodeDL, "DEBUGGameFrameEnd");

		Result.IsValid = Result.UpdateAndRender && Result.GetSoundSamples;
	}
	else
	{
		printf("OSXLoadGameCode: error loading %s\n", TempDLName);
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
		// TODO(casey): Currently, we never unload libraries, because
		// we may still be pointing to strings that are inside them
		// (despite our best efforts). Should we just make "never unload"
		// be the policy?
		//
		//dlclose(GameCode->GameCodeDL);
		//
		GameCode->GameCodeDL = 0;
	}

	GameCode->IsValid = false;
	GameCode->UpdateAndRender = 0;
	GameCode->GetSoundSamples = 0;
}

