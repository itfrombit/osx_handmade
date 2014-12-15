/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Jeff Buck $
   $Notice: (C) Copyright 2014. All Rights Reserved. $
   ======================================================================== */

/*
	TODO(jeff): THIS IS NOT A FINAL PLATFORM LAYER!!!

	This will be updated to keep parity with Casey's win32 platform layer.
	See his win32_handmade.cpp file for TODO details.
*/


#include <Cocoa/Cocoa.h>

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/glu.h>
#import <CoreVideo/CVDisplayLink.h>

#import <AudioUnit/AudioUnit.h>
#import <IOKit/hid/IOHIDLib.h>

#include <sys/stat.h>	// fstat()

#include <mach/mach_time.h>

#include "osx_handmade.h"


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-dereference"
#pragma clang diagnostic ignored "-Wc++11-compat-deprecated-writable-strings"
#include "handmade.h"
#include "handmade.cpp"
#pragma clang diagnostic pop

#include "osx_handmade.cpp"
#include "HandmadeView.h"


internal inline uint64
rdtsc()
{
	uint32 eax = 0;
	uint32 edx;

	__asm__ __volatile__("cpuid;"
			     "rdtsc;"
				: "+a" (eax), "=d" (edx)
				:
				: "%rcx", "%rbx", "memory");

	__asm__ __volatile__("xorl %%eax, %%eax;"
			     "cpuid;"
				:
				:
				: "%rax", "%rbx", "%rcx", "%rdx", "memory");

	return (((uint64)edx << 32) | eax);
}



global_variable AudioUnit	GlobalAudioUnit;
global_variable double		GlobalAudioUnitRenderPhase;

global_variable Float64		GlobalFrequency = 800.0;
global_variable Float64		GlobalSampleRate = 48000.0;


OSStatus SineWaveRenderCallback(void * inRefCon,
                                AudioUnitRenderActionFlags * ioActionFlags,
                                const AudioTimeStamp * inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList * ioData)
{
	#pragma unused(ioActionFlags)
	#pragma unused(inTimeStamp)
	#pragma unused(inBusNumber)

	double currentPhase = *((double*)inRefCon);
	Float32* outputBuffer = (Float32 *)ioData->mBuffers[0].mData;
	const double phaseStep = (GlobalFrequency / GlobalSampleRate) * (2.0 * M_PI);

	for (UInt32 i = 0; i < inNumberFrames; i++)
	{
		outputBuffer[i] = sin(currentPhase);
		currentPhase += phaseStep;
	}

	// Copy to the stereo (or the additional X.1 channels)
	for(UInt32 i = 1; i < ioData->mNumberBuffers; i++)
	{
		memcpy(ioData->mBuffers[i].mData, outputBuffer, ioData->mBuffers[i].mDataByteSize);
	}

	*((double *)inRefCon) = currentPhase;

	return noErr;
}


void OSXInitCoreAudio()
{
	AudioComponentDescription acd;
	acd.componentType         = kAudioUnitType_Output;
	acd.componentSubType      = kAudioUnitSubType_DefaultOutput;
	acd.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent outputComponent = AudioComponentFindNext(NULL, &acd);

	AudioComponentInstanceNew(outputComponent, &GlobalAudioUnit);
	AudioUnitInitialize(GlobalAudioUnit);

	// NOTE(jeff): Make this stereo
	AudioStreamBasicDescription asbd;
	asbd.mSampleRate       = GlobalSampleRate;
	asbd.mFormatID         = kAudioFormatLinearPCM;
	asbd.mFormatFlags      = kAudioFormatFlagsNativeFloatPacked;
	asbd.mChannelsPerFrame = 1;
	asbd.mFramesPerPacket  = 1;
	asbd.mBitsPerChannel   = 1 * sizeof(Float32) * 8;
	asbd.mBytesPerPacket   = 1 * sizeof(Float32);
	asbd.mBytesPerFrame    = 1 * sizeof(Float32);

	// TODO(jeff): Add some error checking...
	AudioUnitSetProperty(GlobalAudioUnit,
                         kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input,
                         0,
                         &asbd,
                         sizeof(asbd));

	AURenderCallbackStruct cb;
	cb.inputProc       = SineWaveRenderCallback;
	cb.inputProcRefCon = &GlobalAudioUnitRenderPhase;

	AudioUnitSetProperty(GlobalAudioUnit,
                         kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Global,
                         0,
                         &cb,
                         sizeof(cb));

	AudioOutputUnitStart(GlobalAudioUnit);
}


void OSXStopCoreAudio()
{
	NSLog(@"Stopping Core Audio");
	AudioOutputUnitStop(GlobalAudioUnit);
	AudioUnitUninitialize(GlobalAudioUnit);
	AudioComponentInstanceDispose(GlobalAudioUnit);
}


void OSXHIDAdded(void* context, IOReturn result, void* sender, IOHIDDeviceRef device)
{
	#pragma unused(context)
	#pragma unused(result)
	#pragma unused(sender)
	#pragma unused(device)

	NSLog(@"Gamepad was plugged in");
}

void OSXHIDRemoved(void* context, IOReturn result, void* sender, IOHIDDeviceRef device)
{
	#pragma unused(context)
	#pragma unused(result)
	#pragma unused(sender)
	#pragma unused(device)

	NSLog(@"Gamepad was unplugged");
}

void OSXHIDAction(void* context, IOReturn result, void* sender, IOHIDValueRef value)
{
	#pragma unused(result)
	#pragma unused(sender)

	IOHIDElementRef element = IOHIDValueGetElement(value);
	if (CFGetTypeID(element) != IOHIDElementGetTypeID())
	{
		return;
	}

	IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
	IOHIDElementType type = IOHIDElementGetType(element);
	CFStringRef name = IOHIDElementGetName(element);
	int usagePage = IOHIDElementGetUsagePage(element);
	int usage = IOHIDElementGetUsage(element);

	CFIndex elementValue = IOHIDValueGetIntegerValue(value);

	// NOTE(jeff): This is the pointer back to our view
	HandmadeView* view = (__bridge HandmadeView*)context;

	// NOTE(jeff): This is just for reference. From the USB HID Usage Tables spec:
	// Usage Pages:
	//   1 - Generic Desktop (mouse, joystick)
	//   2 - Simulation Controls
	//   3 - VR Controls
	//   4 - Sports Controls
	//   5 - Game Controls
	//   6 - Generic Device Controls (battery, wireless, security code)
	//   7 - Keyboard/Keypad
	//   8 - LED
	//   9 - Button
	//   A - Ordinal
	//   B - Telephony
	//   C - Consumer
	//   D - Digitizers
	//  10 - Unicode
	//  14 - Alphanumeric Display
	//  40 - Medical Instrument

	// TODO(jeff): Fix this hardcoded scaling!
	int hatDelta = 16;
	int xyRange = 256;
	int xyHalfRange = xyRange / 2;
	int xyScalingShiftFactor = 3;

	if (usagePage == 1) // Generic Desktop Page
	{
		switch(usage)
		{
			case 0x30: // x
				view->_hidX = (int)(elementValue - xyHalfRange) >> xyScalingShiftFactor;
				break;


			case 0x31: // y
				view->_hidY = (int)(elementValue - xyHalfRange) >> xyScalingShiftFactor;
				break;

			case 0x32: // z
				view->_hidX = (int)(elementValue - xyHalfRange) >> xyScalingShiftFactor;
				break;

			case 0x35: // rz
				view->_hidY = (int)(elementValue - xyHalfRange) >> xyScalingShiftFactor;
				break;

			case 0x39: // Hat 0 = up, 2 = right, 4 = down, 6 = left, 8 = centered
			{
				switch(elementValue)
				{
					case 0:
						view->_hidX = 0;
						view->_hidY = -hatDelta;
						break;

					case 1:
						view->_hidX = hatDelta;
						view->_hidY = -hatDelta;
						break;

					case 2:
						view->_hidX = hatDelta;
						view->_hidY = 0;
						break;

					case 3:
						view->_hidX = hatDelta;
						view->_hidY = hatDelta;
						break;

					case 4:
						view->_hidX = 0;
						view->_hidY = hatDelta;
						break;

					case 5:
						view->_hidX = -hatDelta;
						view->_hidY = hatDelta;
						break;

					case 6:
						view->_hidX = -hatDelta;
						view->_hidY = 0;
						break;

					case 7:
						view->_hidX = -hatDelta;
						view->_hidY = -hatDelta;
						break;

					case 8:
						view->_hidX = 0;
						view->_hidY = 0;
						break;
				}

			} break;

			default:
				//NSLog(@"Gamepad Element: %@  Type: %d  Page: %d  Usage: %d  Name: %@  Cookie: %i  Value: %ld  _hidX: %d",
				//      element, type, usagePage, usage, name, cookie, elementValue, view->_hidX);
				break;
		}
	}
	else if (usagePage == 7) // Keyboard
	{
		// NOTE(jeff): usages 0-3:
		//   0 - Reserved
		//   1 - ErrorRollOver
		//   2 - POSTFail
		//   3 - ErrorUndefined
		// Ignore them for now...
		if (usage < 4) return;

		NSString* keyName = @"";

		// TODO(jeff): Store the keyboard events somewhere...

		switch(usage)
		{
			case kHIDUsage_KeyboardW:
				keyName = @"w";
				break;

			case kHIDUsage_KeyboardA:
				keyName = @"a";
				break;

			case kHIDUsage_KeyboardS:
				keyName = @"s";
				break;

			case kHIDUsage_KeyboardD:
				keyName = @"d";
				break;

			case kHIDUsage_KeyboardQ:
				keyName = @"q";
				break;

			case kHIDUsage_KeyboardE:
				keyName = @"e";
				break;

			case kHIDUsage_KeyboardSpacebar:
				keyName = @"Space";
				break;

			case kHIDUsage_KeyboardEscape:
				keyName = @"ESC";
				break;

			case kHIDUsage_KeyboardUpArrow:
				keyName = @"Up";
				break;

			case kHIDUsage_KeyboardLeftArrow:
				keyName = @"Left";
				break;

			case kHIDUsage_KeyboardDownArrow:
				keyName = @"Down";
				break;

			case kHIDUsage_KeyboardRightArrow:
				keyName = @"Right";
				break;

			default:
				return;
				break;
		}
		if (elementValue == 1)
		{
			NSLog(@"%@ pressed", keyName);
		}
		else if (elementValue == 0)
		{
			NSLog(@"%@ released", keyName);
		}
	}
	else if (usagePage == 9) // Buttons
	{
		if (elementValue == 1)
		{
			view->_hidButtons[usage] = 1;
			NSLog(@"Button %d pressed", usage);
		}
		else if (elementValue == 0)
		{
			view->_hidButtons[usage] = 0;
			NSLog(@"Button %d released", usage);
		}
		else
		{
			NSLog(@"Gamepad Element: %@  Type: %d  Page: %d  Usage: %d  Name: %@  Cookie: %i  Value: %ld  _hidX: %d",
				  element, type, usagePage, usage, name, cookie, elementValue, view->_hidX);
		}
	}
	else
	{
		NSLog(@"Gamepad Element: %@  Type: %d  Page: %d  Usage: %d  Name: %@  Cookie: %i  Value: %ld  _hidX: %d",
			  element, type, usagePage, usage, name, cookie, elementValue, view->_hidX);
	}
}


@implementation HandmadeView

-(void)setupGamepad
{
	_hidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

	if (_hidManager)
	{
		// NOTE(jeff): We're asking for Joysticks, GamePads, Multiaxis Controllers
		// and Keyboards
		NSArray* criteria = @[ @{ (NSString*)CFSTR(kIOHIDDeviceUsagePageKey):
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								(NSString*)CFSTR(kIOHIDDeviceUsageKey):
									[NSNumber numberWithInt:kHIDUsage_GD_Joystick]
								},
							@{ (NSString*)CFSTR(kIOHIDDeviceUsagePageKey):
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								(NSString*)CFSTR(kIOHIDDeviceUsageKey):
									[NSNumber numberWithInt:kHIDUsage_GD_GamePad]
								},
							@{ (NSString*)CFSTR(kIOHIDDeviceUsagePageKey):
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								(NSString*)CFSTR(kIOHIDDeviceUsageKey):
									[NSNumber numberWithInt:kHIDUsage_GD_MultiAxisController]
								},
							@{ (NSString*)CFSTR(kIOHIDDeviceUsagePageKey):
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								(NSString*)CFSTR(kIOHIDDeviceUsageKey):
									[NSNumber numberWithInt:kHIDUsage_GD_Keyboard]
								}
							];

		// NOTE(jeff): These all return void, so no error checking...
		IOHIDManagerSetDeviceMatchingMultiple(_hidManager, (__bridge CFArrayRef)criteria);
		IOHIDManagerRegisterDeviceMatchingCallback(_hidManager, OSXHIDAdded, (__bridge void*)self);
		IOHIDManagerRegisterDeviceRemovalCallback(_hidManager, OSXHIDRemoved, (__bridge void*)self);
		IOHIDManagerScheduleWithRunLoop(_hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

		if (IOHIDManagerOpen(_hidManager, kIOHIDOptionsTypeNone) == kIOReturnSuccess)
		{
			IOHIDManagerRegisterInputValueCallback(_hidManager, OSXHIDAction, (__bridge void*)self);
		}
		else
		{
			// TODO(jeff): Diagnostic
		}
	}
	else
	{
		// TODO(jeff): Diagnostic
	}
}


- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime
{
	// NOTE(jeff): We'll probably use this outputTime later for more precise
	// drawing, but ignore it for now
	#pragma unused(outputTime)

    @autoreleasepool
    {
		[self drawView:NO];
    }

    return kCVReturnSuccess;
}


// Renderer callback function
static CVReturn GLXViewDisplayLinkCallback(CVDisplayLinkRef displayLink,
                                           const CVTimeStamp* now,
                                           const CVTimeStamp* outputTime,
                                           CVOptionFlags inFlags,
                                           CVOptionFlags* outFlags,
                                           void* displayLinkContext)
{
	#pragma unused(displayLink)
	#pragma unused(now)
	#pragma unused(inFlags)
	#pragma unused(outFlags)

    CVReturn result = [(__bridge HandmadeView*)displayLinkContext getFrameForTime:outputTime];
    return result;
}


- (void)setup
{
	if (_setupComplete)
	{
		return;
	}

	// Allocate Memory
	_gameMemory.PermanentStorageSize = Megabytes(64);
	_gameMemory.TransientStorageSize = Gigabytes(4);
	
	uint64 totalSize = _gameMemory.PermanentStorageSize + _gameMemory.TransientStorageSize;
	kern_return_t result = vm_allocate((vm_map_t)mach_task_self(),
									   (vm_address_t*)&_gameMemory.PermanentStorage,
									   totalSize,
									   VM_FLAGS_ANYWHERE);
	if (result != KERN_SUCCESS)
	{
		// TODO(jeff): Diagnostic
		NSLog(@"Error allocating memory");
	}
	
	_gameMemory.TransientStorage = ((uint8*)_gameMemory.PermanentStorage
								   + _gameMemory.PermanentStorageSize);
	
	
	// Get the conversion factor for doing profile timing with mach_absolute_time()
	mach_timebase_info_data_t timebase;
	mach_timebase_info(&timebase);
	_machTimebaseConversionFactor = (double)timebase.numer / (double)timebase.denom;
	
	[self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    NSOpenGLPixelFormatAttribute attrs[] =
    {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFADoubleBuffer,
        //NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
    NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

    if (pf == nil)
    {
        NSLog(@"No OpenGL pixel format");
    }

    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
    [self setPixelFormat:pf];
    [self setOpenGLContext:context];

	_fullScreenOptions = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES]
													 forKey:NSFullScreenModeSetting];

	int BytesPerPixel = 4;
	_renderBuffer.Width = 800;
	_renderBuffer.Height = 600;
	_renderBuffer.Memory = (uint8*)malloc(_renderBuffer.Width * _renderBuffer.Height * 4);
	_renderBuffer.Pitch = _renderBuffer.Width * BytesPerPixel;

	//_cols = 800;
	//_rows = 600;
	//_renderbuffer = (unsigned char*)malloc(_cols * _rows * 4);

	[self setupGamepad];

	OSXInitCoreAudio();

	_setupComplete = YES;
}


- (id)init
{
	self = [super init];

	if (self == nil)
	{
		return nil;
	}

	[self setup];

	return self;
}


- (void)awakeFromNib
{
	[self setup];
}


- (void)prepareOpenGL
{
    [super prepareOpenGL];

    [[self openGLContext] makeCurrentContext];

    // NOTE(jeff): Use the vertical refresh rate to sync buffer swaps
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &_textureId);
    glBindTexture(GL_TEXTURE_2D, _textureId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _renderBuffer.Width, _renderBuffer.Height,
				 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE /*GL_MODULATE*/);

    CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
    CVDisplayLinkSetOutputCallback(_displayLink, &GLXViewDisplayLinkCallback, (__bridge void *)(self));

    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);

    CVDisplayLinkStart(_displayLink);
}


- (void)reshape
{
    [super reshape];

	[self drawView:YES];
}


- (void)drawView:(BOOL)resize
{
	// NOTE(jeff): Drawing is normally done on a background thread via CVDisplayLink.
	// When the window/view is resized, reshape is called automatically on the
	// main thread, so lock the context from simultaneous access during a resize.
	CGLLockContext([[self openGLContext] CGLContextObj]);

	uint64 LastCycleCount = rdtsc();
	uint64 StartTime = mach_absolute_time();

	if (resize)
	{
		// In case we're called from reshape
		NSRect rect = [self bounds];

		glDisable(GL_DEPTH_TEST);
		glLoadIdentity();
		glViewport(0, 0, rect.size.width, rect.size.height);
	}
	else
	{
		// NOTE(jeff): Don't run the game logic during resize events

		// TODO(jeff): Fix this for multiple controllers
		local_persist game_input Input[2] = {};
		local_persist game_input* NewInput = &Input[0];
		local_persist game_input* OldInput = &Input[1];

		game_controller_input* OldController = &OldInput->Controllers[0];
		game_controller_input* NewController = &NewInput->Controllers[0];

	#if 0
		OldController->IsAnalog = true;
		OldController->StartX = OldController->EndX;
		OldController->StartY = OldController->EndY;
		OldController->EndX = _hidX;
		OldController->EndY = _hidY;

		OldController->Down.EndedDown = _hidButtons[1];
		OldController->Up.EndedDown = _hidButtons[2];
		OldController->Left.EndedDown = _hidButtons[3];
		OldController->Right.EndedDown = _hidButtons[4];
	#endif

		NewController->IsAnalog = true;
		NewController->StartX = OldController->EndX;
		NewController->StartY = OldController->EndY;
		NewController->EndX = _hidX;
		NewController->EndY = _hidY;

		NewController->Down.EndedDown = _hidButtons[1];
		NewController->Up.EndedDown = _hidButtons[2];
		NewController->Left.EndedDown = _hidButtons[3];
		NewController->Right.EndedDown = _hidButtons[4];


		GameUpdateAndRender(&_gameMemory, NewInput, &_renderBuffer, &_soundBuffer);

		
		// TODO(jeff): Move this into the game render code
		GlobalFrequency = 440.0 + (32 * _hidY);

		game_input* Temp = NewInput;
		NewInput = OldInput;
		OldInput = Temp;
	}
	
	[[self openGLContext] makeCurrentContext];
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	GLfloat vertices[] =
	{
		-1, 1, 0,
		-1, -1, 0,
		1, -1, 0,
		1, 1, 0
	};

	GLfloat tex_coords[] =
	{
		0, 1,
		0, 0,
		1, 0,
		1, 1,
	};

    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindTexture(GL_TEXTURE_2D, _textureId);

    glEnable(GL_TEXTURE_2D);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _renderBuffer.Width, _renderBuffer.Height,
					GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, _renderBuffer.Memory);
	
    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
    glColor4f(1,1,1,1);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    glDisable(GL_TEXTURE_2D);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    CGLUnlockContext([[self openGLContext] CGLContextObj]);

	
	uint64 EndCycleCount = rdtsc();
	uint64 EndTime = mach_absolute_time();
	
	//uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
	
	real64 MSPerFrame = (real64)(EndTime - StartTime) * _machTimebaseConversionFactor / 1.0E6;
	real64 SPerFrame = MSPerFrame / 1000.0;
	//real64 FPS = 1.0 / SPerFrame;
	
	// NSLog(@"%.02fms/f,  %.02ff/s", MSPerFrame, FPS);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"
- (void)dealloc
{
	OSXStopCoreAudio();

	// NOTE(jeff): It's a good idea to stop the display link before
	// anything in the view is released. Otherwise, the display link
	// might try calling into the view for an update after the view's
	// memory is released.
    CVDisplayLinkStop(_displayLink);
    CVDisplayLinkRelease(_displayLink);

    //[super dealloc];
}
#pragma clang diagnostic pop


- (void)toggleFullScreen:(id)sender
{
	#pragma unused(sender)

	if ([self isInFullScreenMode])
	{
		[self exitFullScreenModeWithOptions:_fullScreenOptions];
		[[self window] makeFirstResponder:self];
	}
	else
	{
		[self enterFullScreenMode:[NSScreen mainScreen]
					  withOptions:_fullScreenOptions];
	}
}


- (BOOL) acceptsFirstResponder
{
	return YES;
}


- (BOOL) becomeFirstResponder
{
	return  YES;
}


- (BOOL) resignFirstResponder
{
	return YES;
}

@end


