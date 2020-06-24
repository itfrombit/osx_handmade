#include <Cocoa/Cocoa.h>

#define GL_GLEXT_LEGACY
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#include <dlfcn.h>

#import "handmade_types.h"
#import "handmade_intrinsics.h"
#import "handmade_math.h"
#import "handmade_shared.h"
#import "handmade_renderer.h"
#import "handmade_image.h"

#import "osx_handmade_opengl.h"
#import "handmade_renderer_opengl.h"

#import "osx_handmade_cocoa.h"
#import "osx_handmade_renderer.h"

NSOpenGLContext* OSXInitOpenGLView(NSWindow* Window);
#import "osx_handmade_opengl.cpp"

#import "handmade_renderer_opengl.cpp"
#import "handmade_image.cpp"


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

NSOpenGLContext* InternalGLContext;

- (void)reshape
{
	[super reshape];

	NSRect bounds = [self bounds];
	[InternalGLContext makeCurrentContext];
	[InternalGLContext update];
	glViewport(0, 0, bounds.size.width, bounds.size.height);
}

@end


NSOpenGLContext* OSXInitOpenGLView(NSWindow* Window)
{
	NSView* CV = [Window contentView];

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
    NSOpenGLContext* GLContext = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:NULL];

	InternalGLContext = GLContext;

	HandmadeView* GLView = [[HandmadeView alloc] init];
	[GLView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[GLView setPixelFormat:PixelFormat];
	[GLView setOpenGLContext:GLContext];
	[GLView setFrame:[CV bounds]];

	[CV addSubview:GLView];

    [PixelFormat release];

	///////////////////////////////////////////////////////////////////
	// OpenGL setup with Cocoa
#if HANDMADE_USE_VSYNC
    GLint swapInt = 1;
#else
    GLint swapInt = 0;
#endif
	[GLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

	[GLContext setView:[Window contentView]];
	[GLContext makeCurrentContext];

	return GLContext;
}


#if 0
NSWindow* OSXInitOpenGLWindow(OSXCocoaContext* CocoaContext, NSString* AppName,
                              float WindowWidth, float WindowHeight)
{
	NSRect ScreenRect = [[NSScreen mainScreen] frame];

	NSRect InitialFrame = NSMakeRect((ScreenRect.size.width - WindowWidth) * 0.5,
									(ScreenRect.size.height - WindowHeight) * 0.5,
									WindowWidth,
									WindowHeight);

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
#endif

