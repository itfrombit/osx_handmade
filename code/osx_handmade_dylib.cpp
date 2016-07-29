///////////////////////////////////////////////////////////////////////
// osx_handmade_dylib.cpp
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//


osx_game_code OSXLoadGameCode(const char* SourceDLName)
{
	osx_game_code Result = {};

	// TODO(casey): Need to get the proper path here!
	// TODO(casey): Automatic determination of when updates are necessary

	Result.DLLastWriteTime = OSXGetLastWriteTime(SourceDLName);

	Result.GameCodeDL = dlopen(SourceDLName, RTLD_LAZY|RTLD_GLOBAL);
	if (Result.GameCodeDL)
	{
		Result.UpdateAndRender = (game_update_and_render*)
			dlsym(Result.GameCodeDL, "GameUpdateAndRender");

		Result.GetSoundSamples = (game_get_sound_samples*)
			dlsym(Result.GameCodeDL, "GameGetSoundSamples");

		Result.DEBUGFrameEnd = (debug_game_frame_end*)
			dlsym(Result.GameCodeDL, "DEBUGGameFrameEnd");

		Result.IsValid = Result.UpdateAndRender && Result.GetSoundSamples;
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
		dlclose(GameCode->GameCodeDL);
		GameCode->GameCodeDL = 0;
	}

	GameCode->IsValid = false;
	GameCode->UpdateAndRender = 0;
	GameCode->GetSoundSamples = 0;
}

