///////////////////////////////////////////////////////////////////////
// osx_main.mm
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

#include <Cocoa/Cocoa.h>

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/glu.h>

#include <mach/mach_time.h>


#import "handmade_platform.h"
#import "handmade_memory.h"
#import "osx_handmade.h"



// Let the command line override
#ifndef HANDMADE_USE_VSYNC
#define HANDMADE_USE_VSYNC 1
#endif


global_variable NSOpenGLContext* GlobalGLContext;
global_variable r32 GlobalAspectRatio;


///////////////////////////////////////////////////////////////////////
// Application Delegate

@interface HandmadeAppDelegate : NSObject<NSApplicationDelegate, NSWindowDelegate>
@end


@implementation HandmadeAppDelegate


///////////////////////////////////////////////////////////////////////
// Application delegate methods
//
- (void)applicationDidFinishLaunching:(id)sender
{
	#pragma unused(sender)
}


- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	#pragma unused(sender)

	return YES;
}


- (void)applicationWillTerminate:(NSApplication*)sender
{
	#pragma unused(sender)

	printf("applicationWillTerminate\n");
}



///////////////////////////////////////////////////////////////////////
// Window delegate methods
//
- (void)windowWillClose:(id)sender
{
	printf("Main window will close. Stopping game.\n");

	OSXStopGame();
}


- (NSSize)windowWillResize:(NSWindow*)window
                    toSize:(NSSize)frameSize
{
	// Maintain the proper aspect ratio...
	frameSize.height = frameSize.width / GlobalAspectRatio;

	return frameSize;
}


- (void)windowDidResize:(NSNotification*)notification
{
	NSWindow* window = [notification object];
	NSRect frame = [window frame];

	[GlobalGLContext update];

	// OpenGL reshape. Typically done in the view
	glDisable(GL_DEPTH_TEST);
	glLoadIdentity();
	glViewport(0, 0, frame.size.width, frame.size.height);
}

@end
//
// Application Delegate
///////////////////////////////////////////////////////////////////////


void OSXCreateMainMenu()
{
	// Create the Menu. Two options right now:
	//   Toggle Full Screen
	//   Quit
	NSMenu* menubar = [NSMenu new]; 

	NSMenuItem* appMenuItem = [NSMenuItem new];
	[menubar addItem:appMenuItem];

	[NSApp setMainMenu:menubar];

	NSMenu* appMenu = [NSMenu new];

    //NSString* appName = [[NSProcessInfo processInfo] processName];
    NSString* appName = @"Handmade Hero";


    NSString* toggleFullScreenTitle = @"Toggle Full Screen";
    NSMenuItem* toggleFullScreenMenuItem = [[NSMenuItem alloc] initWithTitle:toggleFullScreenTitle
    											 action:@selector(toggleFullScreen:)
    									  keyEquivalent:@"f"];
    [appMenu addItem:toggleFullScreenMenuItem];

    NSString* quitTitle = [@"Quit " stringByAppendingString:appName];
    NSMenuItem* quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
    											 action:@selector(terminate:)
    									  keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
}


void OSXProcessPendingMessages(osx_game_data* GameData)
{
	NSEvent* Event;

	do
	{
		Event = [NSApp nextEventMatchingMask:NSAnyEventMask
									untilDate:nil
										inMode:NSDefaultRunLoopMode
									dequeue:YES];

		switch ([Event type])
		{
			case NSKeyDown:
			{
				unichar C = [[Event charactersIgnoringModifiers] characterAtIndex:0];
				int ModifierFlags = [Event modifierFlags];
				int CommandKeyFlag = ModifierFlags & NSCommandKeyMask;
				int ControlKeyFlag = ModifierFlags & NSControlKeyMask;
				int AlternateKeyFlag = ModifierFlags & NSAlternateKeyMask;

				int KeyDownFlag = 1;
				
				OSXKeyProcessing(KeyDownFlag, C,
								 CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag,
								 GameData->NewInput, GameData);
			} break;

			case NSKeyUp:
			{
				unichar C = [[Event charactersIgnoringModifiers] characterAtIndex:0];
				int ModifierFlags = [Event modifierFlags];
				int CommandKeyFlag = ModifierFlags & NSCommandKeyMask;
				int ControlKeyFlag = ModifierFlags & NSControlKeyMask;
				int AlternateKeyFlag = ModifierFlags & NSAlternateKeyMask;

				int KeyDownFlag = 0;

				OSXKeyProcessing(KeyDownFlag, C,
								 CommandKeyFlag, ControlKeyFlag, AlternateKeyFlag,
								 GameData->NewInput, GameData);
			} break;

			default:
				[NSApp sendEvent:Event];
		}
	} while (Event != nil);
}


///////////////////////////////////////////////////////////////////////
// Startup

int main(int argc, const char* argv[])
{
	#pragma unused(argc)
	#pragma unused(argv)

	//return NSApplicationMain(argc, argv);
	@autoreleasepool
	{

	///////////////////////////////////////////////////////////////////
	// NSApplication
	//
	NSApplication* app = [NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];


	OSXCreateMainMenu();


	///////////////////////////////////////////////////////////////////
	// Set working directory 
	//
	NSString *dir = [[NSFileManager defaultManager] currentDirectoryPath];
	NSLog(@"working directory: %@", dir);

	NSFileManager* FileManager = [NSFileManager defaultManager];
	NSString* AppPath = [NSString stringWithFormat:@"%@/Contents/Resources",
		[[NSBundle mainBundle] bundlePath]];
	if ([FileManager changeCurrentDirectoryPath:AppPath] == NO)
	{
		Assert(0);
	}


	HandmadeAppDelegate* appDelegate = [[HandmadeAppDelegate alloc] init];
	[app setDelegate:appDelegate];

    [NSApp finishLaunching];


	///////////////////////////////////////////////////////////////////
	// Game Setup
	//
    NSOpenGLPixelFormatAttribute openGLAttributes[] =
    {
        NSOpenGLPFAAccelerated,
#if HANDMADE_USE_VSYNC
        NSOpenGLPFADoubleBuffer, // Uses vsync
#endif
        NSOpenGLPFADepthSize, 24,
        //NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
	NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:openGLAttributes];
    GlobalGLContext = [[NSOpenGLContext alloc] initWithFormat:format shareContext:NULL];
    [format release];

	DEBUGSetEventRecording(true);

	// game_data holds the OS X platform layer non-Cocoa data structures
	osx_game_data GameData = {};


	OSXSetupGameData(&GameData, [GlobalGLContext CGLContextObj]);

	///////////////////////////////////////////////////////////////////
	// NSWindow and NSOpenGLView
	//
	// Create the main window and the content view
	NSRect screenRect = [[NSScreen mainScreen] frame];
	float Width = 1920.0;
	float Height = 1080.0;
	//float Width = 960.0;
	//float Height = 540.0;
	int BytesPerPixel = 4;

	GlobalAspectRatio = Width / Height;

	NSRect frame = NSMakeRect((screenRect.size.width - Width) * 0.5,
	                          (screenRect.size.height - Height) * 0.5,
	                          Width,
	                          Height);

	NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
										 styleMask:NSTitledWindowMask
											               | NSClosableWindowMask
											               | NSMiniaturizableWindowMask
											               | NSResizableWindowMask
										   backing:NSBackingStoreBuffered
										     defer:NO];

	[window setBackgroundColor: NSColor.blackColor];
	[window setDelegate:appDelegate];

	[window setMinSize:NSMakeSize(100, 100)];
	[window setTitle:@"Handmade Hero"];
	[window makeKeyAndOrderFront:nil];


	OSXSetupGameRenderBuffer(&GameData, Width, Height, BytesPerPixel);


	///////////////////////////////////////////////////////////////////
	//
	// OpenGL setup with Cocoa
	//
#if HANDMADE_USE_VSYNC
    GLint swapInt = 1;
#else
    GLint swapInt = 0;
#endif
	[GlobalGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	[GlobalGLContext setView:[window contentView]];

	[GlobalGLContext makeCurrentContext];

	///////////////////////////////////////////////////////////////////
	// Non-Cocoa OpenGL
	//
	OSXSetupOpenGL(&GameData);


#if 0
	// Default to full screen mode at startup...
    NSDictionary* fullScreenOptions =
    	[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES]
								    forKey:NSFullScreenModeSetting];

	[[window contentView] enterFullScreenMode:[NSScreen mainScreen]
				                  withOptions:fullScreenOptions];
#endif


	///////////////////////////////////////////////////////////////////
	// Run loop 
	//
	//uint tickCounter = 0;
	u64 CurrentTime = mach_absolute_time();
	GameData.LastCounter = CurrentTime;
	float frameTime = 0.0f;

	while (OSXIsGameRunning())
	{
		OSXProcessPendingMessages(&GameData);

		[GlobalGLContext makeCurrentContext];


		///////////////////////////////////////////////////////////////
		// Main Game Function
		//
		CGRect WindowFrame = [window frame];

		CGPoint MouseLocationInScreen = [NSEvent mouseLocation];
		BOOL MouseInWindowFlag = NSPointInRect(MouseLocationInScreen, WindowFrame);
		CGPoint MouseLocationInView = {};

		if (MouseInWindowFlag)
		{
			// NOTE(jeff): Use this instead of convertRectFromScreen: if you want to support Snow Leopard
			// NSPoint PointInWindow = [[self window] convertScreenToBase:[NSEvent mouseLocation]];

			// We don't actually care what the mouse screen coordinates are, we just want the
			// coordinates relative to the content view
			NSRect RectInWindow = [window convertRectFromScreen:NSMakeRect(MouseLocationInScreen.x,
																		   MouseLocationInScreen.y,
																		   1, 1)];
			NSPoint PointInWindow = RectInWindow.origin;
			MouseLocationInView = [[window contentView] convertPoint:PointInWindow fromView:nil];
		}

		u32 MouseButtonMask = [NSEvent pressedMouseButtons];

		OSXProcessFrameAndRunGameLogic(&GameData, WindowFrame,
									   MouseInWindowFlag, MouseLocationInView,
									   MouseButtonMask);

		// flushes and forces vsync
#if HANDMADE_USE_VSYNC
		BEGIN_BLOCK("SwapBuffers");
		[GlobalGLContext flushBuffer];
		END_BLOCK();
#else
		glFlush();
#endif

		u64 EndCounter = mach_absolute_time();

		FRAME_MARKER(OSXGetSecondsElapsed(GameData.LastCounter, EndCounter));

		frameTime += OSXGetSecondsElapsed(GameData.LastCounter, EndCounter);
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

