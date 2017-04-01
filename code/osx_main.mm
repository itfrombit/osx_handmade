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

#define Maximum(A, B) ((A > B) ? (A) : (B))

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
	//frameSize.height = frameSize.width / GlobalAspectRatio;

	NSRect WindowRect = [window frame];
	NSRect ContentRect = [window contentRectForFrameRect:WindowRect];

	r32 WidthAdd = (WindowRect.size.width - ContentRect.size.width);
	r32 HeightAdd = (WindowRect.size.height - ContentRect.size.height);

	r32 RenderWidth = 1920;
	r32 RenderHeight = 1080;
	//r32 RenderWidth = 2560;
	//r32 RenderHeight = 1440;

	r32 NewCy = (RenderHeight * (frameSize.width - WidthAdd)) / RenderWidth;

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
	//float Width = 3840.0;
	//float Height = 2160.0;
	//float Width = 2560.0;
	//float Height = 1440.0;
	float Width = 1920.0;
	float Height = 1080.0;
	//float Width = 960.0;
	//float Height = 540.0;
	int BytesPerPixel = 4;

	GlobalAspectRatio = Width / Height;

	NSRect InitialFrame = NSMakeRect((screenRect.size.width - Width) * 0.5,
									(screenRect.size.height - Height) * 0.5,
									Width,
									Height);

	NSWindow* window = [[NSWindow alloc] initWithContentRect:InitialFrame
									styleMask:NSTitledWindowMask
												| NSClosableWindowMask
												| NSMiniaturizableWindowMask
												| NSResizableWindowMask
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

