///////////////////////////////////////////////////////////////////////
// vim: set filetype=objcpp :
//
// osx_tab_view.mm
//
// Jeff Buck
// Copyright 2018. All Rights Reserved.
//
// A quick and dirty way to view the output of an hhaedit dump.
//
// Some notes:
//
//  - Create .hha dump files for viewing like this:
//    hhaedit -dump ../data/intro_art.hha > intro_art.hha.dump
//
//  - Run this utility (currently compiled as HandmadeTabView.app).
//    Double-click the .app file in the Finder, or if you're a
//    command line user, run it like this from a shell prompt:
//        open HandmadeTabView.app
//
//  - The application launches with an empty document. You can drag
//    and drop a dump file from the finder onto any existing window
//    (empty or not) and it will replace the current contents
//    with the contents of the dropped file.
//    You can also use the standard Open File or Open Recent menu
//    items to select a file.
//
//  - The application supports multiple files open at the same time
//    so that you can compare dump file contents.
//
//  - 'Command +' and 'Command -' (also available from the View menu)
//    will expand/collapse all nodes of the dump tree in the currently
//    active window.
//
//  - 'Command r' will reload the file contents of the currently
//    active window.


#include <Cocoa/Cocoa.h>
#include <sys/stat.h>

///////////////////////////////////////////////////////////////////////
// Sean Barrett's stretchy buffers
// See https://github.com/nothings/stb for details and other
// very nice single file C libraries.

#define sb_free   stb_sb_free
#define sb_push   stb_sb_push
#define sb_count  stb_sb_count
#define sb_add    stb_sb_add
#define sb_last   stb_sb_last

#define stb_sb_free(a)         ((a) ? free(stb__sbraw(a)),0 : 0)
#define stb_sb_push(a,v)       (stb__sbmaybegrow(a,1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_count(a)        ((a) ? stb__sbn(a) : 0)
#define stb_sb_add(a,n)        (stb__sbmaybegrow(a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define stb_sb_last(a)         ((a)[stb__sbn(a)-1])

#define stb__sbraw(a) ((int *) (a) - 2)
#define stb__sbm(a)   stb__sbraw(a)[0]
#define stb__sbn(a)   stb__sbraw(a)[1]

#define stb__sbneedgrow(a,n)  ((a)==0 || stb__sbn(a)+(n) >= stb__sbm(a))
#define stb__sbmaybegrow(a,n) (stb__sbneedgrow(a,(n)) ? stb__sbgrow(a,n) : 0)
#define stb__sbgrow(a,n)      (*((void **)&(a)) = stb__sbgrowf((a), (n), sizeof(*(a))))

#include <stdlib.h>

static void * stb__sbgrowf(void *arr, int increment, int itemsize)
{
	int dbl_cur = arr ? 2*stb__sbm(arr) : 0;
	int min_needed = stb_sb_count(arr) + increment;
	int m = dbl_cur > min_needed ? dbl_cur : min_needed;
	int *p = (int *) realloc(arr ? stb__sbraw(arr) : 0, itemsize * m + sizeof(int)*2);

	if (p)
	{
		if (!arr)
			p[1] = 0;
		p[0] = m;
		return p + 2;
	}
	else
	{
#ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
		STRETCHY_BUFFER_OUT_OF_MEMORY;
#endif
		return (void *)(2*sizeof(int)); // try to force a NULL pointer exception later
	}
}

// end of the stretchy buffers
///////////////////////////////////////////////////////////////////////


const float WindowWidth = 800.0f;
const float WindowHeight = 600.0f;


typedef struct OutlineNode
{
	int index;
	int level;
	char* name;
	int child_count;
	int* children;
} OutlineNode;


const int MAX_INDENT_LEVELS = 256;

// NOTE(jeff): This is totally wasteful. Use a hash table!
int whitespace_to_indent_level[MAX_INDENT_LEVELS];

// NOTE(jeff): This one is okay
int current_node_idx_for_level[MAX_INDENT_LEVELS];


OutlineNode* LoadOutline(const char* filename)
{
	whitespace_to_indent_level[0] = 0;
	for (int i = 1; i < MAX_INDENT_LEVELS; ++i)
	{
		whitespace_to_indent_level[i] = -1;
		current_node_idx_for_level[i] = -1;
	}

	struct stat stat_buffer;
	if (stat(filename, &stat_buffer) != 0)
	{
		printf("Could not stat file: %s\n", filename);
		return NULL;
	}

	off_t file_size = stat_buffer.st_size;

	char* buffer = (char*)malloc(file_size + 1);

	FILE* fp = fopen(filename, "r");
	if (fp == NULL)
	{
		printf("Could not open file: %s\n", filename);
		free(buffer);
		return NULL;
	}

	if (fread(buffer, file_size, 1, fp) != 1)
	{
		printf("Error reading file: %s\n", filename);
		fclose(fp);
		free(buffer);
		return NULL;
	}

	fclose(fp);

	buffer[stat_buffer.st_size] = 0;
	char* buffer_end = buffer + stat_buffer.st_size;

	OutlineNode* outline = NULL;
	int n = 0;
	int cur_level = 0;

	char* cur_buf = buffer;

	while (*cur_buf && (cur_buf <= buffer_end))
	{
		int ws = 0;
		while (*cur_buf && isspace(*cur_buf))
		{
			++ws;
			++cur_buf;
		}

		char* line_text_begin = cur_buf;

		while (*cur_buf && (*cur_buf != '\n'))
		{
			++cur_buf;
		}

		*cur_buf++ = 0;

		assert(ws < MAX_INDENT_LEVELS);

		int level = whitespace_to_indent_level[ws];
		if (level == -1)
		{
			// first time we've seen this level
			level = cur_level + 1;
			whitespace_to_indent_level[ws] = level;
		}

		OutlineNode node = (OutlineNode){n, level, line_text_begin, 0, NULL};
		sb_push(outline, node);

		assert(level < MAX_INDENT_LEVELS);

		current_node_idx_for_level[level] = n;

		if (level > 0)
		{
			int parent_idx = current_node_idx_for_level[level-1];
			++outline[parent_idx].child_count;
			sb_push(outline[parent_idx].children, n);
		}

		cur_level = level;

		++n;
	}

#if 0
	// Debug output
	for (int i = 0; i < n; ++i)
	{
		printf("[%d] [%d] [%d]: %s\n", outline[i].index, outline[i].level, outline[i].child_count, outline[i].name);

		for (int j = 0; j < outline[i].child_count; ++j)
		{
			printf("    %d\n", outline[i].children[j]);
		}
	}
#endif

	return outline;
}


///////////////////////////////////////////////////////////////////////
// WindowController

NSOutlineView* OSXCreateOutlineView(NSWindow* parent)
{
	// NOTE(jeff):
	// There's some funky view hierarchy mess to get straight.
	// You can figure this out by looking at a sample in
	// Interface Builder in Xcode.
	//
	// Window
	//   Window's ContentView
	//     ScrollView
	//       ClipView
	//         OutlineView
	//
	// Once autoresizing is set up, we can pretty much ignore
	// the ScrollView and ClipView.

	NSView* cv = [parent contentView];

	NSScrollView* scrollv = [[NSScrollView alloc] initWithFrame:[cv bounds]];
	scrollv.hasVerticalScroller = YES;
	scrollv.hasHorizontalScroller = YES;
	scrollv.wantsLayer = YES;
	scrollv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

	NSClipView* clipv = [[NSClipView alloc] init];
	clipv.autoresizesSubviews = YES;
	clipv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

	NSOutlineView* ov = [[NSOutlineView alloc] init];
	ov.selectionHighlightStyle = NSTableViewSelectionHighlightStyleSourceList;
	ov.floatsGroupRows = NO;
	ov.indentationPerLevel = 16.f;
	ov.indentationMarkerFollowsCell = NO;

	NSTableColumn* col1 = [[NSTableColumn alloc] initWithIdentifier:@"node"];
	col1.editable = NO;
	col1.width = 500.0f;
	col1.headerCell.stringValue = [NSString stringWithFormat:@"Node"];
	[ov addTableColumn:col1];
	[ov setOutlineTableColumn:col1];

	NSTableColumn* col2 = [[NSTableColumn alloc] initWithIdentifier:@"children"];
	col2.editable = NO;
	col2.headerCell.stringValue = @"Children";
	[ov addTableColumn:col2];

	scrollv.contentView = clipv;
	clipv.documentView = ov;

	[cv addSubview:scrollv];

	return ov;
}


///////////////////////////////////////////////////////////////////////

@interface TabDocument : NSDocument
{
@public
	NSString* _filename;
	OutlineNode* _outline;
}

- (OutlineNode*)outline;
- (NSString*)filename;
@end


///////////////////////////////////////////////////////////////////////

@interface TabViewWindowController : NSWindowController<NSOutlineViewDataSource, NSOutlineViewDelegate, NSWindowDelegate>
{
	NSOutlineView* _ov;
}

- (void)droppedFile:(NSString*)filename;
- (void)reloadData:(id)sender;
@end


///////////////////////////////////////////////////////////////////////
// TabDocument

@implementation TabDocument

- (OutlineNode*)outline { return _outline; }
- (NSString*)filename { return _filename; }

- (void)makeWindowControllers
{
	TabViewWindowController* wc = [[TabViewWindowController alloc] init];

	[self addWindowController:wc];
	[wc reloadData:self];
}


- (BOOL)loadFile:(NSString*)filename
{
	if (filename == nil)
	{
		filename = _filename;
	}

	const char* c_filename = [filename UTF8String];
	OutlineNode* newOutline = LoadOutline(c_filename);

	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:filename]];

	if (outline)
	{
		sb_free(outline);
	}

	_outline = newOutline;
	_filename = filename;

	return YES;
}


// How to intercept loading a file without using Cocoa's default archiving/unarchiving.
- (BOOL)readFromURL:(NSURL*)url ofType:(NSString*)type error:(NSError**)error
{
	BOOL result = [self loadFile:[url path]];
	return result;
}
@end


///////////////////////////////////////////////////////////////////////
// TabWindow - override to support drag and drop

@interface TabWindow : NSWindow
{
	TabViewWindowController* _wc;
}

- (void)setWindowController:(TabViewWindowController*)wc;
@end


@implementation TabWindow

- (void)setWindowController:(TabViewWindowController*)wc
{
	_wc = wc;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
	return NSDragOperationCopy;
}


- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
	return NSDragOperationCopy;
}


- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
	NSPasteboard* pb = [sender draggingPasteboard];
	NSArray* filenames = [pb propertyListForType:NSFilenamesPboardType];

	if (filenames.count != 1)
	{
		return NO;
	}

	NSString* filename = [filenames objectAtIndex:0];
	NSLog(@"Dropped file: %@", filename);

	[_wc droppedFile:filename];

	return YES;
}

@end


///////////////////////////////////////////////////////////////////////
// TabViewWindowController
//   - override to create a document window without a nib file
//   - acts as the data source and delegate for the OutlineView.

@implementation TabViewWindowController

- (id)init
{
	float width = WindowWidth;
	float height = WindowHeight;

	NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
	float originY = NSMaxY(screenRect) - height - 100.0f;

	NSRect initialFrame = NSMakeRect(100.0f, originY, width, height);

	NSWindow* window = [[TabWindow alloc] initWithContentRect:initialFrame
									styleMask:NSWindowStyleMaskTitled
												| NSWindowStyleMaskClosable
												| NSWindowStyleMaskMiniaturizable
												| NSWindowStyleMaskResizable
									backing:NSBackingStoreBuffered
									defer:NO];

	NSView* cv = [window contentView];
	[cv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[cv setAutoresizesSubviews:YES];

	[window setMinSize:NSMakeSize(160, 90)];

	self = [super initWithWindow:window];

	[self setShouldCascadeWindows:YES];

	_ov = OSXCreateOutlineView(window);
	[_ov setDataSource:self];
	[_ov setDelegate:self];

	[window setWindowController:self];
	[window registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];

	return self;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)reloadData:(id)sender
{
	[_ov reloadData];
}

- (void)droppedFile:(NSString*)filename
{
	TabDocument* doc = [self document];

	[doc loadFile:filename];

	[doc setDisplayName:filename];
	[self synchronizeWindowTitleWithDocumentName];

	[_ov reloadData];
}


///////////////////////////////////////////////////////////////////////
// NSOutlineViewDataSource methods
- (BOOL)outlineView:(NSOutlineView*)ov isItemExpandable:(id)item
{
	OutlineNode* outline = [[self document] outline];

	if (outline == NULL)
	{
		return NO;
	}
	else
	{
		long index = [item longValue];

		return outline[index].child_count > 0;
	}
}


- (NSInteger)outlineView:(NSOutlineView*)ov numberOfChildrenOfItem:(id)item
{
	OutlineNode* outline = [[self document] outline];

	if (outline == NULL)
	{
		return 0;
	}
	else
	{
		long index = [item longValue];

		return outline[index].child_count;
	}
}


- (id)outlineView:(NSOutlineView*)ov child:(NSInteger)index ofItem:(id)item
{
	OutlineNode* outline = [[self document] outline];

	long node_index = [item longValue];
	long child_index = outline[node_index].children[index];

	return [NSNumber numberWithLong:child_index];
}

- (id)outlineView:(NSOutlineView*)ov objectValueForTableColumn:(NSTableColumn*)tableColumn
									           byItem:(id)item
{
	OutlineNode* outline = [[self document] outline];

	long index = [item longValue];

	if ([[tableColumn identifier] isEqualToString:@"node"])
	{
		return [NSString stringWithUTF8String:outline[index].name];
	}
	else if ([[tableColumn identifier] isEqualToString:@"children"])
	{
		int child_count = outline[index].child_count;

		if (child_count == 1)
		{
			return [NSString stringWithFormat:@"1 child"];
		}
		else if (child_count > 1)
		{
			return [NSString stringWithFormat:@"%d children", outline[index].child_count];
		}
	}

	return nil;
}

///////////////////////////////////////////////////////////////////////
// NSOutlineViewDelegate methods

- (BOOL)outlineView:(NSOutlineView*)ov shouldEditTableColumn:(NSTableColumn*)tc item:(id)item
{
	return NO;
}

- (void)reloadFile:(id)sender
{
	// passing nil will reload the current file
	[[self document] loadFile:nil];

	[_ov reloadData];
}

- (void)expandAll:(id)sender
{
	[_ov expandItem:nil expandChildren:YES];
}


- (void)collapseAll:(id)sender
{
	[_ov collapseItem:nil collapseChildren:YES];
}


@end


///////////////////////////////////////////////////////////////////////
// Create an application menu and set targets without a nib file
//
void OSXCreateMainMenu(NSString* appName)
{
    NSMenu* menubar = [NSMenu new];
	[NSApp setMainMenu:menubar];

	// Application Menu

	NSMenuItem* appMenuItem = [NSMenuItem new];
	[menubar addItem:appMenuItem];

	NSMenu* appMenu = [NSMenu new];

	NSString* aboutTitle = [@"About" stringByAppendingString:appName];
	[appMenu addItemWithTitle:aboutTitle action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

	[appMenu addItem:[NSMenuItem separatorItem]];

	NSMenuItem* mi = [appMenu addItemWithTitle:@"Services" action:NULL keyEquivalent:@""];
	NSMenu* servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
	[appMenu setSubmenu:servicesMenu forItem:mi];
	[NSApp setServicesMenu:servicesMenu];

	[appMenu addItem:[NSMenuItem separatorItem]];

	NSString* hideTitle = [@"Hide" stringByAppendingString:appName];
	[appMenu addItemWithTitle:hideTitle action:@selector(hide:) keyEquivalent:@"h"];
	mi = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[mi setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
	[appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

	[appMenu addItem:[NSMenuItem separatorItem]];
    NSString* quitTitle = [@"Quit " stringByAppendingString:appName];
    [appMenu addItemWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];

    [appMenuItem setSubmenu:appMenu];


	// File Menu
	NSMenuItem* fileMenuItem = [menubar addItemWithTitle:@"File" action:NULL keyEquivalent:@""];
	NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];

	mi = [fileMenu addItemWithTitle:@"New" action:@selector(newDocument:) keyEquivalent:@"n"];
	[mi setTarget:[NSDocumentController sharedDocumentController]];
	mi = [fileMenu addItemWithTitle:@"Open..." action:@selector(openDocument:) keyEquivalent:@"o"];
	[mi setTarget:[NSDocumentController sharedDocumentController]];

	mi = [fileMenu addItemWithTitle:@"Open Recent" action:NULL keyEquivalent:@""];
	NSMenu* openRecentMenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
	[openRecentMenu performSelector:@selector(_setMenuName:) withObject:@"NSRecentDocumentsMenu"];
	[fileMenu setSubmenu:openRecentMenu forItem:mi];

	mi = [openRecentMenu addItemWithTitle:@"Clear Menu" action:@selector(clearRecentDocuments:) keyEquivalent:@""];

	[fileMenu addItem:[NSMenuItem separatorItem]];

	mi = [fileMenu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];

	[menubar setSubmenu:fileMenu forItem:fileMenuItem];

	// View Menu
	NSMenuItem* viewMenuItem = [menubar addItemWithTitle:@"View" action:NULL keyEquivalent:@""];
	NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];

	[viewMenu addItemWithTitle:@"Reload File" action:@selector(reloadFile:) keyEquivalent:@"r"];
	[viewMenu addItem:[NSMenuItem separatorItem]];
    [viewMenu addItemWithTitle:@"Expand All" action:@selector(expandAll:) keyEquivalent:@"+"];
    [viewMenu addItemWithTitle:@"Collapse All" action:@selector(collapseAll:) keyEquivalent:@"-"];
	[viewMenu addItem:[NSMenuItem separatorItem]];
    [viewMenu addItemWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];

	[menubar setSubmenu:viewMenu forItem:viewMenuItem];

	// Window Menu
	NSMenuItem* windowMenuItem = [menubar addItemWithTitle:@"Window" action:NULL keyEquivalent:@""];
	NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
	[windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""];

	[menubar setSubmenu:windowMenu forItem:windowMenuItem];

	// Help Menu
	NSMenuItem* helpMenuItem = [menubar addItemWithTitle:@"Help" action:NULL keyEquivalent:@""];
	NSMenu* helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];

	[helpMenu addItemWithTitle:@"Help" action:@selector(showHelp:) keyEquivalent:@"?"];
	[menubar setSubmenu:helpMenu forItem:helpMenuItem];
}


///////////////////////////////////////////////////////////////////////

int main(int argc, const char* argv[])
{
	@autoreleasepool
	{
	NSApplication* app = [NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	OSXCreateMainMenu(@"HandmadeTabView");

	[NSApp finishLaunching];

	[app setDelegate:[NSDocumentController sharedDocumentController]];

	[app run];
	}
}

