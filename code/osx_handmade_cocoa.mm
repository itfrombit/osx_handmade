
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



OSXCocoaContext OSXInitCocoaContext(NSString* AppName, float WindowWidth, float WindowHeight)
{
	OSXCocoaContext Context = {};

	// Cocoa boilerplate startup code
	NSApplication* app = [NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	// Set working directory
	NSString *dir = [[NSFileManager defaultManager] currentDirectoryPath];

	NSFileManager* FileManager = [NSFileManager defaultManager];
	Context.WorkingDirectory = [NSString stringWithFormat:@"%@/Contents/Resources",
		[[NSBundle mainBundle] bundlePath]];
	if ([FileManager changeCurrentDirectoryPath:Context.WorkingDirectory] == NO)
	{
		Assert(0);
	}
	NSLog(@"working directory: %@", Context.WorkingDirectory);

	Context.AppDelegate = [[HandmadeAppDelegate alloc] init];
	[app setDelegate:Context.AppDelegate];

    [NSApp finishLaunching];

	// Create the main application window
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
	[Window setDelegate:Context.AppDelegate];

	NSView* CV = [Window contentView];
	[CV setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[CV setAutoresizesSubviews:YES];

	[Window setMinSize:NSMakeSize(160, 90)];
	[Window setTitle:AppName];
	[Window makeKeyAndOrderFront:nil];

	Context.Window = Window;
	Context.AppName = AppName;

	return Context;
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


void OSXMessageBox(NSString* Title, NSString* Text)
{
	NSAlert* MsgBox = [[NSAlert alloc] init];
	[MsgBox addButtonWithTitle:@"Quit"];
	[MsgBox setMessageText:Title];
	[MsgBox setInformativeText:Text];
	[MsgBox setAlertStyle:NSAlertStyleWarning];

	[MsgBox runModal];
}
