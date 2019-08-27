///////////////////////////////////////////////////////////////////////
// osx_main.mm
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

#include <Cocoa/Cocoa.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libproc.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/vm_map.h>
#include <mach-o/dyld.h>
#include <libkern/OSAtomic.h> // for OSMemoryBarrier
#include <pthread.h>

extern "C"
{
	__attribute__((visibility("default"))) unsigned long NvOptimusEnablement = 1;
	__attribute__((visibility("default"))) int AmdPowerXpressRequestHighPerformance = 1;
}

// Let the command line override
#ifndef HANDMADE_USE_VSYNC
#define HANDMADE_USE_VSYNC 1
#endif

#if 0
#define GL_GLEXT_LEGACY
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#endif

#define Maximum(A, B) ((A > B) ? (A) : (B))

#import "handmade_platform.h"
#import "handmade_intrinsics.h"
#import "handmade_math.h"

#import "handmade_shared.h"
#import "handmade_memory.h"
#import "handmade_renderer.h"

#import "osx_handmade_events.h"
#import "osx_handmade.h"
#import "osx_handmade_cocoa.h"
#import "osx_handmade_renderer.h"

platform_api Platform;

osx_state GlobalOSXState;
global b32 GlobalSoftwareRendering;
global GLuint OpenGLDefaultInternalTextureFormat;

global b32 GlobalRunning = 1;
global b32 GlobalPause;
global v2 GlobalAspectRatio = {16.0, 9.0};

v2 DefaultWindowDimension =
{
	//192.0, 108.0
	//480.0, 270.0
	//960.0, 540.0
	//1280.0, 720.0
	//1279.0, 719.0
	1920.0, 1080.0
	//2560.0, 1440.0
};


#if 0
const r32 DefaultWindowWidth = 960;
const r32 DefaultWindowHeight = 540;

const r32 GlobalRenderWidth = 960;
const r32 GlobalRenderHeight = 540;
#endif

#import "handmade_renderer.cpp"

#import "osx_handmade_memory.cpp"
#import "osx_handmade_debug.cpp"
#import "osx_handmade_file.cpp"
#import "osx_handmade_process.cpp"
#import "osx_handmade_thread.cpp"
#import "osx_handmade_audio.cpp"
#import "osx_handmade_hid.cpp"
#import "osx_handmade_dylib.cpp"
#import "osx_handmade_playback.cpp"
#import "osx_handmade_game.cpp"

// Generic Cocoa boilerplate NSApplicationDelegate, NSWindow, NSOpenGLView classes
#import "osx_handmade_cocoa.mm"


void OSXProcessPendingMessages(osx_game_data* GameData)
{
	NSEvent* Event;

	ZeroStruct(GameData->NewInput->FKeyPressed);
	memcpy(GameData->OldKeyboardState, GameData->KeyboardState, sizeof(GameData->KeyboardState));

	do
	{
		Event = [NSApp nextEventMatchingMask:NSAnyEventMask
									untilDate:nil
										inMode:NSDefaultRunLoopMode
									dequeue:YES];

		switch ([Event type])
		{
			case NSKeyDown:
			case NSKeyUp:
			{
				u32 KeyCode = [Event keyCode];
				unichar C = [[Event charactersIgnoringModifiers] characterAtIndex:0];
				u32 ModifierFlags = [Event modifierFlags];
				int CommandKeyFlag = (ModifierFlags & NSCommandKeyMask) > 0;
				int ControlKeyFlag = (ModifierFlags & NSControlKeyMask) > 0;
				int AlternateKeyFlag = (ModifierFlags & NSAlternateKeyMask) > 0;
				int ShiftKeyFlag = (ModifierFlags & NSShiftKeyMask) > 0;

				int KeyDownFlag = ([Event type] == NSKeyDown) ? 1 : 0;
				GameData->KeyboardState[KeyCode] = KeyDownFlag;

				//printf("%s: keyCode: %d  unichar: %c\n", NSKeyDown ? "KeyDown" : "KeyUp", [Event keyCode], C);

				OSXKeyProcessing(KeyDownFlag, KeyCode, C,
								ShiftKeyFlag, CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag,
								GameData->NewInput, GameData);
			} break;

			case NSFlagsChanged:
			{
				u32 KeyCode = 0;
				u32 ModifierFlags = [Event modifierFlags];
				int CommandKeyFlag = (ModifierFlags & NSCommandKeyMask) > 0;
				int ControlKeyFlag = (ModifierFlags & NSControlKeyMask) > 0;
				int AlternateKeyFlag = (ModifierFlags & NSAlternateKeyMask) > 0;
				int ShiftKeyFlag = (ModifierFlags & NSShiftKeyMask) > 0;

				GameData->KeyboardState[kVK_Command] = CommandKeyFlag;
				GameData->KeyboardState[kVK_Control] = ControlKeyFlag;
				GameData->KeyboardState[kVK_Alternate] = AlternateKeyFlag;
				GameData->KeyboardState[kVK_Shift] = ShiftKeyFlag;

				int KeyDownFlag = 0;

				if (CommandKeyFlag != GameData->OldKeyboardState[kVK_Command])
				{
					KeyCode = kVK_Command;

					if (CommandKeyFlag)
					{
						KeyDownFlag = 1;
					}
				}

				if (ControlKeyFlag != GameData->OldKeyboardState[kVK_Control])
				{
					KeyCode = kVK_Control;

					if (ControlKeyFlag)
					{
						KeyDownFlag = 1;
					}
				}

				if (AlternateKeyFlag != GameData->OldKeyboardState[kVK_Alternate])
				{
					KeyCode = kVK_Option;

					if (AlternateKeyFlag)
					{
						KeyDownFlag = 1;
					}
				}

				if (ShiftKeyFlag != GameData->OldKeyboardState[kVK_Shift])
				{
					if (ShiftKeyFlag)
					{
						KeyDownFlag = 1;
					}

					KeyCode = kVK_Shift;
				}

				//printf("Keyboard flags changed: Cmd: %d  Ctrl: %d  Opt: %d  Shift: %d\n",
			//			CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag, ShiftKeyFlag);

				OSXKeyProcessing(-1, KeyCode, KeyCode,
								ShiftKeyFlag, CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag,
								GameData->NewInput, GameData);
			} break;

			default:
				[NSApp sendEvent:Event];
		}
	} while (Event != nil);

}


///////////////////////////////////////////////////////////////////////

int main(int argc, const char* argv[])
{
	#pragma unused(argc)
	#pragma unused(argv)

	@autoreleasepool
	{

	NSString* AppName = @"Handmade Hero";
	OSXCocoaContext OSXAppContext = OSXInitCocoaContext(AppName,
			DefaultWindowDimension.Width,
			DefaultWindowDimension.Height);

	OSXCreateSimpleMainMenu(AppName);

	DEBUGSetEventRecording(true);

	// game_data holds the OS X platform layer non-Cocoa data structures
	osx_game_data GameData = {};
	OSXSetupGameData(OSXAppContext.Window, &GameData);


	///////////////////////////////////////////////////////////////////
	// Run loop
	//

	//u64 tickCounter = 0;

	u64 CurrentTime = mach_absolute_time();
	GameData.LastCounter = CurrentTime;
	float frameTime = 0.0f;

	while (OSXIsGameRunning())
	{
		OSXInitializeGameInputForNewFrame(&GameData);

		OSXProcessPendingMessages(&GameData);


		///////////////////////////////////////////////////////////////
		// Main Game Function
		//
		osx_mouse_data MouseData = OSXGetMouseData(&OSXAppContext);

		CGRect ContentViewFrame = [[OSXAppContext.Window contentView] frame];

		OSXProcessFrameAndRunGameLogic(&GameData, ContentViewFrame, &MouseData);

		u64 EndCounter = mach_absolute_time();

		f32 MeasuredSecondsPerFrame = OSXGetSecondsElapsed(GameData.LastCounter, EndCounter);
		f32 ExactTargetFramesPerUpdate = MeasuredSecondsPerFrame * (f32)GameData.MonitorRefreshHz;
		u32 NewExpectedFramesPerUpdate = RoundReal32ToInt32(ExactTargetFramesPerUpdate);
		GameData.ExpectedFramesPerUpdate = NewExpectedFramesPerUpdate;

		GameData.TargetSecondsPerFrame = MeasuredSecondsPerFrame;

		FRAME_MARKER(MeasuredSecondsPerFrame);

		frameTime += MeasuredSecondsPerFrame;
		GameData.LastCounter = EndCounter;

#if 0
		++tickCounter;
		if (tickCounter == 60)
		{
			float avgFrameTime = frameTime / 60.0f;
			tickCounter = 0;
			frameTime = 0.0f;
			//printf("frame time = %f\n", avgFrameTime);
		}
#endif
	}


	OSXStopCoreAudio(&GameData.SoundOutput);


	printf("Handmade Hero finished running\n");


	} // @autoreleasepool
}

