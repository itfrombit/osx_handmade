#pragma once

#define MAX_HID_BUTTONS 32


// TODO(jeff): Temporary NSObject for testing.
// Replace with simple struct in a set of hash tables.
@interface HandmadeHIDElement : NSObject
{
@public
	long	type;
	long	page;
	long	usage;
	long	min;
	long	max;
};

- (id)initWithType:(long)type usagePage:(long)p usage:(long)u min:(long)n max:(long)x;

@end


@interface HandmadeView : NSOpenGLView
{
@public
	// display
	CVDisplayLinkRef			_displayLink;
	
	// graphics
	NSDictionary*				_fullScreenOptions;
	GLuint						_textureId;
	//unsigned int				_rows;
	//unsigned int				_cols;
	//unsigned char*			_renderbuffer;
	
	// input
	IOHIDManagerRef				_hidManager;
	int							_hidX;
	int							_hidY;
	uint8						_hidButtons[MAX_HID_BUTTONS];

	game_memory					_gameMemory;
	game_sound_output_buffer	_soundBuffer;
	game_offscreen_buffer		_renderBuffer;


	real64						_machTimebaseConversionFactor;
	BOOL						_setupComplete;

	// TODO(jeff): Replace with set of simple hash tables of structs
	NSMutableDictionary*		_elementDictionary;
}
@end
