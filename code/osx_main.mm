///////////////////////////////////////////////////////////////////////
// osx_main.mm
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

#include <Cocoa/Cocoa.h>

#define GL_GLEXT_LEGACY
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

#include <mach/mach_time.h>
#include <dlfcn.h>

#define Maximum(A, B) ((A > B) ? (A) : (B))

#import "handmade_platform.h"
#import "handmade_intrinsics.h"
#import "handmade_math.h"

#import "handmade_shared.h"
#import "handmade_memory.h"
#import "handmade_renderer.h"
#import "handmade_renderer_opengl.h"

#import "osx_handmade_events.h"
#import "osx_handmade.h"


#import "osx_handmade.cpp"

// Let the command line override
#ifndef HANDMADE_USE_VSYNC
#define HANDMADE_USE_VSYNC 1
#endif


global NSOpenGLContext* GlobalGLContext;
global r32 GlobalAspectRatio;

//const r32 GlobalRenderWidth = 2560.0f;
//const r32 GlobalRenderHeight = 1600.0f;

//const r32 GlobalRenderWidth = 2560.0f;
//const r32 GlobalRenderHeight = 1440.0f;

//const r32 GlobalRenderWidth = 1920.0f;
//const r32 GlobalRenderHeight = 1080.0f;

const r32 GlobalRenderWidth = 960;
const r32 GlobalRenderHeight = 540;

//const r32 GlobalRenderWidth = 480;
//const r32 GlobalRenderHeight = 270;

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
	//frameSize.height = frameSize.width / GlobalAspectRatio;

	NSRect WindowRect = [window frame];
	NSRect ContentRect = [window contentRectForFrameRect:WindowRect];

	r32 WidthAdd = (WindowRect.size.width - ContentRect.size.width);
	r32 HeightAdd = (WindowRect.size.height - ContentRect.size.height);


	r32 NewCy = (GlobalRenderHeight * (frameSize.width - WidthAdd)) / GlobalRenderWidth;

	frameSize.height = NewCy + HeightAdd;

	return frameSize;
}


#if 0
- (void)windowDidResize:(NSNotification*)notification
{
	NSWindow* window = [notification object];

	//Assert(GlobalGLContext == [NSOpenGLContext currentContext]);
	//[GlobalGLContext update];
	[GlobalGLContext makeCurrentContext];
	[GlobalGLContext update];

	// OpenGL reshape. Typically done in the view
	//glLoadIdentity();

	NSRect ContentViewFrame = [[window contentView] frame];
	glViewport(0, 0, ContentViewFrame.size.width, ContentViewFrame.size.height);
	[GlobalGLContext update];
}
#endif

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
@interface HandmadeView : NSOpenGLView
{
}
@end

@implementation HandmadeView

- (id)init
{
	self = [super init];

	return self;
}


- (void)prepareOpenGL
{
	[super prepareOpenGL];
	[[self openGLContext] makeCurrentContext];
}

- (void)reshape
{
	[super reshape];

	NSRect bounds = [self bounds];
	//[[self openGLContext] makeCurrentContext];
	[GlobalGLContext makeCurrentContext];
	[GlobalGLContext update];
	glViewport(0, 0, bounds.size.width, bounds.size.height);

#if 0
	printf("HandmadeView reshape: [%.0f, %.0f] [%.0f, %.0f]\n",
			bounds.origin.x, bounds.origin.y,
			bounds.size.width, bounds.size.height);

	glLoadIdentity();
	glClearColor(1.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
#endif
}


@end


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
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
	NSOpenGLPixelFormat* PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:openGLAttributes];
    GlobalGLContext = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:NULL];

	DEBUGSetEventRecording(true);

	// game_data holds the OS X platform layer non-Cocoa data structures
	osx_game_data GameData = {};


	OSXSetupGameData(&GameData, [GlobalGLContext CGLContextObj]);

	///////////////////////////////////////////////////////////////////
	// NSWindow and NSOpenGLView
	//
	// Create the main window and the content view
	NSRect screenRect = [[NSScreen mainScreen] frame];

#if 0
	//float Width = 3840.0;
	//float Height = 2160.0;

	//float Width = 2560.0;
	//float Height = 1440.0;

	float Width = 1920.0;
	float Height = 1080.0;

	//float Width = 960.0;
	//float Height = 540.0;

	//float Width = 480.0;
	//float Height = 270.0;
#endif

	int BytesPerPixel = 4;

	GlobalAspectRatio = GlobalRenderWidth / GlobalRenderHeight;

	NSRect InitialFrame = NSMakeRect((screenRect.size.width - GlobalRenderWidth) * 0.5,
									(screenRect.size.height - GlobalRenderHeight) * 0.5,
									GlobalRenderWidth,
									GlobalRenderHeight);

	NSWindow* window = [[NSWindow alloc] initWithContentRect:InitialFrame
									styleMask:NSWindowStyleMaskTitled
												| NSWindowStyleMaskClosable
												| NSWindowStyleMaskMiniaturizable
												| NSWindowStyleMaskResizable
									backing:NSBackingStoreBuffered
									defer:NO];

	[window setBackgroundColor: NSColor.blackColor];
	[window setDelegate:appDelegate];

	NSView* cv = [window contentView];
	[cv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[cv setAutoresizesSubviews:YES];

#if 1
	HandmadeView* GLView = [[HandmadeView alloc] init];
	[GLView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[GLView setPixelFormat:PixelFormat];
	[GLView setOpenGLContext:GlobalGLContext];
	[GLView setFrame:[cv bounds]];

	[cv addSubview:GLView];
#endif

    [PixelFormat release];

	[window setMinSize:NSMakeSize(160, 90)];
	[window setTitle:@"Handmade Hero"];
	[window makeKeyAndOrderFront:nil];


	OSXSetupGameRenderBuffer(&GameData, GlobalRenderWidth, GlobalRenderHeight, BytesPerPixel);


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
	OSXInitOpenGL();


#if 0
	// Default to full screen mode at startup...
    NSDictionary* fullScreenOptions =
		[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES] forKey:NSFullScreenModeSetting];

	[[window contentView] enterFullScreenMode:[NSScreen mainScreen] withOptions:fullScreenOptions];
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
		OSXInitializeGameInputForNewFrame(&GameData);

		OSXProcessPendingMessages(&GameData);

		[GlobalGLContext makeCurrentContext];


		///////////////////////////////////////////////////////////////
		// Main Game Function
		//
		CGRect WindowFrame = [window frame];
		CGRect ContentViewFrame = [[window contentView] frame];

#if 0
		printf("WindowFrame: [%.0f, %.0f]", WindowFrame.size.width, WindowFrame.size.height);
		printf("    ContentFrame: [%.0f, %.0f]\n", ContentViewFrame.size.width, ContentViewFrame.size.height);
#endif

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

		OSXProcessFrameAndRunGameLogic(&GameData, ContentViewFrame,
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

