//
//  EQGLView.m
//  handmade
//
//  Created by Jeff on 11/21/14.
//  Copyright (c) 2014 Jeff Buck. All rights reserved.
//
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/glu.h>
#import <CoreVideo/CVDisplayLink.h>
#import <CoreVideo/CoreVideo.h>

//#import <OpenGL/CGLCurrent.h>
//#import <OpenGL/CGLContext.h>
//#import <AppKit/NSOpenGLView.h>

#define UNUSED(x) (void)x

#import <AudioUnit/AudioUnit.h>
#import <IOKit/hid/IOHIDLib.h>


#import "EQGLView.h"

//static const float EQ_DEFAULT_REFRESH_RATE = 60.0;

AudioUnit	audioUnit;
double		audioUnitRenderPhase;

Float32*	audioBuffer;
int			audioBufferSize;
int			audioBufferIndex;

Float64 frequency = 800;
Float64 sampleRate = 48000;
double duration = 1000.0;
double riseFallTime = 5.0;
double gain = 0.8;


//	audioBuffer = (Float32*)malloc(sizeof(Float32) * 2 * sampleRate * duration / 1000.0);
//	audioBufferIndex = 0;

// This is our render callback. It will be called very frequently for short
// buffers of audio (512 samples per call on my machine).
OSStatus SineWaveRenderCallback(void * inRefCon,
                                AudioUnitRenderActionFlags * ioActionFlags,
                                const AudioTimeStamp * inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList * ioData)
{
	UNUSED(ioActionFlags);
	UNUSED(inTimeStamp);
	UNUSED(inBusNumber);

    // inRefCon is the context pointer we passed in earlier when setting the render callback
    double currentPhase = *((double *)inRefCon);
    // ioData is where we're supposed to put the audio samples we've created
    Float32 * outputBuffer = (Float32 *)ioData->mBuffers[0].mData;
    const double phaseStep = (frequency / 44100.) * (M_PI * 2.);

    for(UInt32 i = 0; i < inNumberFrames; i++)
    {
		outputBuffer[i] = sin(currentPhase);
		currentPhase += phaseStep;
    }

    // If we were doing stereo (or more), this would copy our sine wave samples
    // to all of the remaining channels
    for(UInt32 i = 1; i < ioData->mNumberBuffers; i++) {
        memcpy(ioData->mBuffers[i].mData, outputBuffer, ioData->mBuffers[i].mDataByteSize);
    }

    // writing the current phase back to inRefCon so we can use it on the next call
    *((double *)inRefCon) = currentPhase;
    return noErr;
}


void initCoreAudio()
{
	//  First, we need to establish which Audio Unit we want.

	//  We start with its description, which is:
    AudioComponentDescription acd = {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple
    };

	//  Next, we get the first (and only) component corresponding to that description
    AudioComponent outputComponent = AudioComponentFindNext(NULL, &acd);

	//  Now we can create an instance of that component, which will create an
	//  instance of the Audio Unit we're looking for (the default output)
    AudioComponentInstanceNew(outputComponent, &audioUnit);
    AudioUnitInitialize(audioUnit);

	//  Next we'll tell the output unit what format our generated audio will
	//  be in. Generally speaking, you'll want to stick to sane formats, since
	//  the output unit won't accept every single possible stream format.
	//  Here, we're specifying floating point samples with a sample rate of
	//  44100 Hz in mono (i.e. 1 channel)
    AudioStreamBasicDescription asbd = {
        .mSampleRate       = sampleRate,
        .mFormatID         = kAudioFormatLinearPCM,
        .mFormatFlags      = kAudioFormatFlagsNativeFloatPacked,
        .mChannelsPerFrame = 1, //1,
        .mFramesPerPacket  = 1,
        .mBitsPerChannel   = 1 * sizeof(Float32) * 8,
        .mBytesPerPacket   = 1 * sizeof(Float32),
        .mBytesPerFrame    = 1 * sizeof(Float32)
    };

    AudioUnitSetProperty(audioUnit,
                         kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input,
                         0,
                         &asbd,
                         sizeof(asbd));

//  Next step is to tell our output unit which function we'd like it
//  to call to get audio samples. We'll also pass in a context pointer,
//  which can be a pointer to anything you need to maintain state between
//  render callbacks. We only need to point to a double which represents
//  the current phase of the sine wave we're creating.
    AURenderCallbackStruct callbackInfo = {
        .inputProc       = SineWaveRenderCallback,
        .inputProcRefCon = &audioUnitRenderPhase
    };

    AudioUnitSetProperty(audioUnit,
                         kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Global,
                         0,
                         &callbackInfo,
                         sizeof(callbackInfo));

//  Here we're telling the output unit to start requesting audio samples
//  from our render callback. This is the line of code that starts actually
//  sending audio to your speakers.
    AudioOutputUnitStart(audioUnit);
}




void generateSine(Float32* buffer,
                  int frequency,
                  double msLength,
                  int sampleRate,
                  double msRiseTime,
                  double gain)
{
	int sampleCount = ((double)sampleRate) * msLength / 1000.0;
	int riseTimeSamples = ((double)sampleRate) * msRiseTime / 1000.0;

	if (gain > 1.0)
	{
		gain = 1.0;
	}
	else if (gain < 0.0)
	{
		gain = 0.0;
	}

	for (int i = 0; i < sampleCount; i += 2)
	{
		double value = sin(2.0 * M_PI * frequency * i / sampleRate);
		if (i < riseTimeSamples)
		{
			value *= sin(i * M_PI / (2.0 * riseTimeSamples));
		}

		if (i < (sampleCount - riseTimeSamples - 1))
		{
			value *= sin(2.0 * M_PI * (i - (sampleCount - riseTimeSamples) + riseTimeSamples) / (4.0 * riseTimeSamples));
		}

		buffer[i] = (value * 32500.0 * gain);
		buffer[i+1] = buffer[i];
	}
}



void stopCoreAudio()
{
	AudioOutputUnitStop(audioUnit);
	AudioUnitUninitialize(audioUnit);
	AudioComponentInstanceDispose(audioUnit);
}




void gamepadWasAdded(void* context, IOReturn result, void* sender, IOHIDDeviceRef device)
{
	UNUSED(context);
	UNUSED(result);
	UNUSED(sender);
	UNUSED(device);

	NSLog(@"Gamepad was plugged in");
}

void gamepadWasRemoved(void* context, IOReturn result, void* sender, IOHIDDeviceRef device)
{
	UNUSED(context);
	UNUSED(result);
	UNUSED(sender);
	UNUSED(device);

	NSLog(@"Gamepad was unplugged");
}

void gamepadAction(void* context, IOReturn result, void* sender, IOHIDValueRef value)
{
	UNUSED(result);
	UNUSED(sender);

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

//	if (usagePage != 1)
//	{
//		NSLog(@"Bad usage page: %d", usagePage);
//		return;
//	}

	CFIndex elementValue = IOHIDValueGetIntegerValue(value);

//	NSLog(@"Gamepad talked: %d / %d - %@ [%i] = %ld", type, usage, name,
//		  cookie, elementValue);

	EQGLView* view = (__bridge EQGLView*)context;

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

	if (usagePage == 1) // Generic Desktop Page
	{
		switch(usage)
		{
			case 0x30: // x
				view->_hidX = (int)(elementValue - 127) >> 3;
				break;


			case 0x31: // y
				view->_hidY = (int)(elementValue - 127) >> 3;
				break;

			case 0x32: // z
				view->_hidX = (int)(elementValue - 127) >> 3;
				break;

			case 0x35: // rz
				view->_hidY = (int)(elementValue - 127) >> 3;
				break;

			case 0x39: // Hat 0 = up, 2 = right, 8 = centered
			{
				switch(elementValue)
				{
					case 0:
						view->_hidX = 0;
						view->_hidY = -16;
						break;

					case 1:
						view->_hidX = 16;
						view->_hidY = -16;
						break;

					case 2:
						view->_hidX = 16;
						view->_hidY = 0;
						break;

					case 3:
						view->_hidX = 16;
						view->_hidY = 16;
						break;

					case 4:
						view->_hidX = 0;
						view->_hidY = 16;
						break;

					case 5:
						view->_hidX = -16;
						view->_hidY = 16;
						break;

					case 6:
						view->_hidX = -16;
						view->_hidY = 0;
						break;

					case 7:
						view->_hidX = -16;
						view->_hidY = -16;
						break;

					case 8:
						view->_hidX = 0;
						view->_hidY = 0;
						break;
				}

			} break;

			default:
				NSLog(@"Gamepad Element: %@  Type: %d  Page: %d  Usage: %d  Name: %@  Cookie: %i  Value: %ld  _hidX: %d",
					  element, type, usagePage, usage, name, cookie, elementValue, view->_hidX);
				break;
		}
	}
	else if (usagePage == 7) // Keyboard
	{
		if (usage < 4) return;

		NSString* keyName = @"";


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
			NSLog(@"Button %d pressed", usage);
		}
		else if (elementValue == 0)
		{
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



//static NSMutableDictionary* create_criterion( UInt32 inUsagePage, UInt32 inUsage )
//{
//	NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
//	[dict setObject: [NSNumber numberWithInt: inUsagePage] forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
//	[dict setObject: [NSNumber numberWithInt: inUsage] forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
//	return dict;
//}




@implementation EQGLView

- (CVReturn)getFrameForTime:(const CVTimeStamp*)outputTime
{
	UNUSED(outputTime);

    @autoreleasepool
    {
        [self drawView];
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
	UNUSED(displayLink);
	UNUSED(now);
	UNUSED(inFlags);
	UNUSED(outFlags);

    CVReturn result = [(__bridge EQGLView*)displayLinkContext getFrameForTime:outputTime];
    return result;
}


-(void) setupGamepad
{
	_hidManager = IOHIDManagerCreate( kCFAllocatorDefault, kIOHIDOptionsTypeNone);

	//NSMutableDictionary* criterion = [[NSMutableDictionary alloc] init];
	//[criterion setObject: [NSNumber numberWithInt: kHIDPage_GenericDesktop] forKey: (NSString*)CFSTR(kIOHIDDeviceUsagePageKey)];
	//[criterion setObject: [NSNumber numberWithInt: kHIDUsage_GD_GamePad] forKey: (NSString*)CFSTR(kIOHIDDeviceUsageKey)];
	//IOHIDManagerSetDeviceMatching(hidManager, (__bridge CFDictionaryRef)criterion);

//	NSArray *criteria2 = [NSArray arrayWithObjects:
//						 create_criterion(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick),
//						 create_criterion(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad),
//						 create_criterion(kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController),
//						 //create_criterion(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard),
//						 nil];

	// TODO: Make this better!

	// NOTE: Make this suck less!
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
	IOHIDManagerSetDeviceMatchingMultiple(_hidManager, (__bridge CFArrayRef)criteria);




	IOHIDManagerRegisterDeviceMatchingCallback(_hidManager, gamepadWasAdded, (__bridge void*)self);
	IOHIDManagerRegisterDeviceRemovalCallback(_hidManager, gamepadWasRemoved, (__bridge void*)self);

	IOHIDManagerScheduleWithRunLoop(_hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOReturn tIOReturn = IOHIDManagerOpen(_hidManager, kIOHIDOptionsTypeNone);
	IOHIDManagerRegisterInputValueCallback(_hidManager, gamepadAction, (__bridge void*)self);
}




- (void)awakeFromNib
{
    NSOpenGLPixelFormatAttribute attrs[] =
    {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFADoubleBuffer,
        //NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        0
    };
    NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

    if (!pf)
    {
        NSLog(@"No OpenGL pixel format");
    }

    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
    [self setPixelFormat:pf];
    [self setOpenGLContext:context];

	_fullScreenOptions = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES]
													 forKey:NSFullScreenModeSetting];

	_cols = 800;
	_rows = 600;

	_renderbuffer = (unsigned char*)malloc(_cols * _rows * 4);

	[self setupGamepad];

	initCoreAudio();
}


- (void)initGL
{
    [[self openGLContext] makeCurrentContext];

    // Synchronize buffer swaps with vertical refresh rate
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &_textureId);
    glBindTexture(GL_TEXTURE_2D, _textureId);

    //NSRect bounds = [self bounds];
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bounds.size.width, bounds.size.height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, _renderbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _rows, _cols, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, _renderbuffer);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    //_renderer = [[GLXOpenGLRenderer alloc] init];
}

- (void)prepareOpenGL
{
    [super prepareOpenGL];

    // Make all the OpenGL calls to setup rendering
    //  and build the necessary rendering objects
    [self initGL];

    // Create a display link capable of being used with all active displays
    CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);

    // Set the renderer output callback function
    CVDisplayLinkSetOutputCallback(_displayLink, &GLXViewDisplayLinkCallback, (__bridge void *)(self));

    // Set the display link for the current renderer
    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);

    // Activate the display link
    CVDisplayLinkStart(_displayLink);
}

- (void)reshape
{
    [super reshape];

    // We draw on a secondary thread through the display link
    // When resizing the view, -reshape is called automatically on the main thread
    // Add a mutex around to avoid the threads accessing the context simultaneously when resizing
    CGLLockContext([[self openGLContext] CGLContextObj]);
    NSRect rect = [self bounds];

    glDisable(GL_DEPTH_TEST);
    glLoadIdentity();
    glViewport(0, 0, rect.size.width, rect.size.height);

    CGLUnlockContext([[self openGLContext] CGLContextObj]);

	NSLog(@"reshape");

    //if (_renderbuffer)
    //{
    //    free(_renderbuffer);
    //}

    //_renderbuffer = (unsigned char*)malloc(rect.size.width * rect.size.height * 4);
}

- (void)render
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLfloat vertices[] = {
        -1, 1, 0,
        -1, -1, 0,
        1, -1, 0,
        1, 1, 0
    };
    GLfloat tex_coords[] = {
        0, 1,
        0, 0,
        1, 0,
        1, 1,
        //0, 1,
    };

//    if (_game.hasFocus)
//    {
//        [_game tick];
//    }
//
//    [_game render];

	//NSRect bounds = [self bounds];

	//static int redOffset = 0;
	static int greenOffset = 0;
	static int blueOffset = 0;

	for (unsigned int y = 0; y < _rows; y++)
	{
		for (unsigned int x = 0; x < _cols; x++)
		{
			unsigned char* pixel = _renderbuffer + (y * _cols + x) * 4;

			pixel[0] = 0;	// a
			pixel[1] = x + blueOffset;	// b
			pixel[2] = y + greenOffset;	// g
			pixel[3] = 0; //x + y + redOffset;	// r
		}
	}

	//++redOffset;
	//++greenOffset;
	//++blueOffset;

	blueOffset += _hidX;
	greenOffset -= _hidY;

	frequency = 440.0 + 8 * _hidY;

    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };


    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _cols, _rows, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, _renderbuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    //GetGLError();
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glColor4f(1,1,1,1);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    glDisable(GL_TEXTURE_2D);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}


- (void) drawView
{
    [[self openGLContext] makeCurrentContext];

    // We draw on a secondary thread through the display link
    // When resizing the view, -reshape is called automatically on the main thread
    // Add a mutex around to avoid the threads accessing the context simultaneously	when resizing
    CGLLockContext([[self openGLContext] CGLContextObj]);

    [self render];

    CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}


- (void) dealloc
{
    // Stop the display link BEFORE releasing anything in the view
    // otherwise the display link thread may call into the view and crash
    // when it encounters something that has been release
    CVDisplayLinkStop(_displayLink);
    CVDisplayLinkRelease(_displayLink);
}


- (IBAction)step:(id)sender
{
	UNUSED(sender);
    //[_game step];
}


- (IBAction)toggleFullScreen:(id)sender
{
	UNUSED(sender);

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

	/*
    if (!_isFullScreen)
    {
        NSDictionary* options = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSNumber numberWithBool:NO], @"NSFullScreenModeAllScreens",
                                 nil];
        [self enterFullScreenMode:self.window.screen withOptions:options];
        _isFullScreen = YES;
    }
    else
    {
        [self exitFullScreenModeWithOptions:nil];
        _isFullScreen = NO;
    }
	 */
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


- (void)mouseDown:(NSEvent*)event
{
    //NSLog(@"mouseDown: %@", event);

    NSRect bounds = [self bounds];

    float xPixelsPerColumn = bounds.size.width / _cols;
    float yPixelsPerRow = bounds.size.height / _rows;

    uint32 row = event.locationInWindow.y / yPixelsPerRow;
    uint32 col = event.locationInWindow.x / xPixelsPerColumn;

    NSLog(@"mouseDown: [%f, %f]  Grid [%d, %d]",
		  event.locationInWindow.x, event.locationInWindow.y,
		  col, row);
}

- (void)mouseMoved:(NSEvent*)e
{
	UNUSED(e);
}



- (void)keyDown:(NSEvent*)e
{
/*
    NSLog(@"KeyDown: %@", e);
	NSString* chars = [e characters];

	if ([chars length])
	{
		unichar key = [[e characters] characterAtIndex:0];

		if (key == 27)
		{
			[self toggleFullScreen:self];
		}
		else
		{
			// forward to something else...
		}
	}
    //[_game keyDown:e];
*/
}

- (void)keyUp:(NSEvent*)e
{
/*
    NSLog(@"KeyUp: %@", e);
    //[_game keyUp:e];
*/
}


@end
