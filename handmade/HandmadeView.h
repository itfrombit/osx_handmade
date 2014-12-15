#pragma once

#define MAX_HID_BUTTONS 32

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
}
@end
