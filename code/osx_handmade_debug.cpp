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


