

struct osx_offscreen_buffer
{
	// NOTE: Pixels are always 32-bits wide. BB GG RR XX
	//BITMAPINFO Info;
	void* Memory;
	int Width;
	int Height;
	int Pitch;
};


struct osx_window_dimension
{
	int Width;
	int Height;
};


struct osx_sound_output
{
	int SamplesPerSecond;
	uint32 RunningSampleIndex;
	int BytesPerSample;
	real32 tSine;
	int LatencySampleCount;
};



#define MAX_HID_BUTTONS 32

@interface HandmadeView : NSOpenGLView
{
@public
	// display
	CVDisplayLinkRef	_displayLink;
	
	// graphics
	NSDictionary*		_fullScreenOptions;
	GLuint				_textureId;
	//unsigned int		_rows;
	//unsigned int		_cols;
	//unsigned char*	_renderbuffer;
	
	// input
	IOHIDManagerRef		_hidManager;
	int					_hidX;
	int					_hidY;
	uint8				_hidButtons[MAX_HID_BUTTONS];
}
@end

