///////////////////////////////////////////////////////////////////////
// osx_handmade_cocoa.h
//
// Jeff Buck
// Copyright 2014-2018. All Rights Reserved.
//


@class HandmadeAppDelegate;

typedef struct osx_mouse_data
{
	b32x MouseInWindowFlag;
	CGPoint MouseLocation; // in view coordinates
	u32 MouseButtonMask;
} osx_mouse_data;


struct OSXCocoaContext
{
	HandmadeAppDelegate* AppDelegate;
	NSWindow* Window;
	NSString* WorkingDirectory;
};


