//
//  EQGLView.h
//  handmade
//
//  Created by Jeff on 11/21/14.
//  Copyright (c) 2014 Jeff. All rights reserved.
//

#import <IOKit/hid/IOHIDLib.h>
#import <Cocoa/Cocoa.h>

@interface EQGLView : NSOpenGLView
{
	//BOOL				_isFullScreen;
	NSDictionary*		_fullScreenOptions;

	CVDisplayLinkRef	_displayLink;
	GLuint				_textureId;

	unsigned int		_rows;
	unsigned int		_cols;
	unsigned char*		_renderbuffer;

@public
	IOHIDManagerRef		_hidManager;
	int					_hidX;
	int					_hidY;
}

- (IBAction)toggleFullScreen:(id)sender;

@end
