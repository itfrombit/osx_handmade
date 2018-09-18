
///////////////////////////////////////////////////////////////////////
// Application Delegate

@interface HandmadeAppDelegate : NSObject<NSApplicationDelegate, NSWindowDelegate>
@end

@implementation HandmadeAppDelegate

// app delegate methods
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
}


// window delegate methods
- (NSSize)windowWillResize:(NSWindow*)window
                    toSize:(NSSize)frameSize
{
	// Maintain the proper aspect ratio...

	NSRect WindowRect = [window frame];
	NSRect ContentRect = [window contentRectForFrameRect:WindowRect];

	r32 WidthAdd = (WindowRect.size.width - ContentRect.size.width);
	r32 HeightAdd = (WindowRect.size.height - ContentRect.size.height);

	r32 NewCy = (GlobalRenderHeight * (frameSize.width - WidthAdd)) / GlobalRenderWidth;

	frameSize.height = NewCy + HeightAdd;

	return frameSize;
}

- (void)windowWillClose:(id)sender
{
	GlobalRunning = false;
}

@end

// Application Delegate
///////////////////////////////////////////////////////////////////////


void OSXCreateSimpleMainMenu(NSString* AppName)
{
    NSMenu* menubar = [NSMenu new];

	NSMenuItem* appMenuItem = [NSMenuItem new];
	[menubar addItem:appMenuItem];

	[NSApp setMainMenu:menubar];

	NSMenu* appMenu = [NSMenu new];

    [appMenu addItem:[[NSMenuItem alloc] initWithTitle:@"Enter Full Screen"
					  						    action:@selector(toggleFullScreen:)
										 keyEquivalent:@"f"]];

    [appMenu addItem:[[NSMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:AppName]
											    action:@selector(terminate:)
										 keyEquivalent:@"q"]];
    [appMenuItem setSubmenu:appMenu];
}


void OSXProcessMinimalPendingMessages()
{
	NSEvent* Event;

	do
	{
		Event = [NSApp nextEventMatchingMask:NSEventMaskAny
									untilDate:nil
										inMode:NSDefaultRunLoopMode
									dequeue:YES];

		switch ([Event type])
		{
			case NSEventTypeKeyDown:
			case NSEventTypeKeyUp:
			{
				u32 KeyCode = [Event keyCode];
				if (KeyCode == kVK_ANSI_Q)
				{
					GlobalRunning = false;
				}
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
	[GlobalGLContext makeCurrentContext];
	[GlobalGLContext update];
	glViewport(0, 0, bounds.size.width, bounds.size.height);
}

@end


OSXCocoaContext OSXInitCocoaContext()
{
	OSXCocoaContext Context = {};

	// Cocoa boilerplate startup code
	NSApplication* app = [NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	// Set working directory
	NSString *dir = [[NSFileManager defaultManager] currentDirectoryPath];
	NSLog(@"working directory: %@", dir);

	NSFileManager* FileManager = [NSFileManager defaultManager];
	Context.WorkingDirectory = [NSString stringWithFormat:@"%@/Contents/Resources",
		[[NSBundle mainBundle] bundlePath]];
	if ([FileManager changeCurrentDirectoryPath:Context.WorkingDirectory] == NO)
	{
		Assert(0);
	}

	Context.AppDelegate = [[HandmadeAppDelegate alloc] init];
	[app setDelegate:Context.AppDelegate];

    [NSApp finishLaunching];

	return Context;
}


NSWindow* OSXInitOpenGLWindow(OSXCocoaContext* CocoaContext, NSString* AppName,
                              float WindowWidth, float WindowHeight)
{
	NSRect ScreenRect = [[NSScreen mainScreen] frame];

	NSRect InitialFrame = NSMakeRect((ScreenRect.size.width - GlobalRenderWidth) * 0.5,
									(ScreenRect.size.height - GlobalRenderHeight) * 0.5,
									GlobalRenderWidth,
									GlobalRenderHeight);

	NSWindow* Window = [[NSWindow alloc] initWithContentRect:InitialFrame
									styleMask:NSWindowStyleMaskTitled
												| NSWindowStyleMaskClosable
												| NSWindowStyleMaskMiniaturizable
												| NSWindowStyleMaskResizable
									backing:NSBackingStoreBuffered
									defer:NO];

	[Window setBackgroundColor: NSColor.redColor];
	[Window setDelegate:CocoaContext->AppDelegate];

	NSView* CV = [Window contentView];
	[CV setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[CV setAutoresizesSubviews:YES];

 	// All hardware NSOpenGLPixelFormats support an sRGB framebuffer.
    NSOpenGLPixelFormatAttribute openGLAttributes[] =
    {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
	NSOpenGLPixelFormat* PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:openGLAttributes];
    GlobalGLContext = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:NULL];

	HandmadeView* GLView = [[HandmadeView alloc] init];
	[GLView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[GLView setPixelFormat:PixelFormat];
	[GLView setOpenGLContext:GlobalGLContext];
	[GLView setFrame:[CV bounds]];

	[CV addSubview:GLView];

    [PixelFormat release];

	[Window setMinSize:NSMakeSize(160, 90)];
	[Window setTitle:AppName];
	[Window makeKeyAndOrderFront:nil];

	///////////////////////////////////////////////////////////////////
	// OpenGL setup with Cocoa
#if HANDMADE_USE_VSYNC
    GLint swapInt = 1;
#else
    GLint swapInt = 0;
#endif
	[GlobalGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	[GlobalGLContext setView:[Window contentView]];
	[GlobalGLContext makeCurrentContext];

	CocoaContext->Window = Window;

	return Window;
}


osx_mouse_data OSXGetMouseData(OSXCocoaContext* AppContext)
{
	osx_mouse_data MouseData = {};

	NSWindow* Window = AppContext->Window;

	CGRect WindowFrame = [Window frame];
	CGRect ContentViewFrame = [[Window contentView] frame];

	CGPoint MouseLocationInScreen = [NSEvent mouseLocation];

	// TODO(jeff): Should this be ContentViewFrame instead of WindowFrame???
	MouseData.MouseInWindowFlag = NSPointInRect(MouseLocationInScreen, WindowFrame);

	if (MouseData.MouseInWindowFlag)
	{
		// We don't actually care what the mouse screen coordinates are, we just want the
		// coordinates relative to the content view
		NSRect RectInWindow = [Window convertRectFromScreen:NSMakeRect(MouseLocationInScreen.x,
																	   MouseLocationInScreen.y,
																	   1, 1)];
		NSPoint PointInWindow = RectInWindow.origin;
		MouseData.MouseLocation = [[Window contentView] convertPoint:PointInWindow fromView:nil];
	}

	MouseData.MouseButtonMask = [NSEvent pressedMouseButtons];

	return MouseData;
}

