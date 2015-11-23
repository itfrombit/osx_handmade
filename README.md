OS X Handmade Hero
==================

This is an ongoing OS X port of Casey Muratori's Handmade Hero.

This repository works with Casey's source code from handmade_hero_219.

This version uses native OS X libraries for the platform layer:
- CoreAudio for sound
- OpenGL for graphics
- IOKit's HID for joystick input

The goal is to be able to drop in Casey's platform independent
game source code and compile and run it unchanged.

This Xcode project first builds a dynamic library subproject
called libhandmade.dylib and includes it in the Handmade Hero
product directory of the application bundle.


Note 2015-11-22:
----------------
This version is compatible with Day 219.

However, the clang/llvm compiler flags some errors in Casey's
Handmade Hero source code that need to be fixed before compiling
under OS X. To fix the source code, run

    sh fix_handmade_hero_source.sh

from a terminal window in the handmade subdirectory
after copying over Casey's source code, but before running 'make'
for the first time.

The shell script is not very robust at finding the lines to change,
so if you are using source code other than Day 219, this may not work.  
If you would rather fix the errors by hand instead of running the above
shell script, here's a summary of the compile errors:

1. handmade_platform.h:501:11 - There's extra junk after the #endif. Just delete it.
2. handmade_generated.h - All of the (u32) casts should be (u64) casts.
3. handmade_debug.cpp - non-portable _snprintf_s functions are not available on OS X. Insert the contents of the provided file 'vsprintf.cpp' near the top of the file (just after the '#include <stdio.h>' line) to fix.
4. handmade_debug.cpp:750 - Change the two (u32) casts to (u64) casts in the first line of the GetOrCreateDebugViewFor() function.


IMPORTANT
---------

Once you clone or update this repository, copy over Casey's .cpp
and .h source files to the handmade subdirectory of this repository.

Also, copy over the test, test2, and test3 asset folders to the
root directory of this repository.

Before you build the application for the first time, you need to
create the packed asset files. To do this, open the separate test_asset_builder
Xcode project and build and run it once. It should find the test 
asset directories and create the .hha files. You only need to
repeat this step when Casey changes the .hha file format, or when he
modifies or adds new assets.


EXTRA IMPORTANT: For better rendering performance, build the project in
Release mode (the default Xcode setting is Debug mode). The Debug mode
version running inside of Xcode may drop audio frames.

You can also set the renderAtHalfSpeed flag in HandmadeView.mm to
reduce the effective rendering rate to 30fps instead of the default
60fps. The proper Core Audio sound buffer size is automatically adjusted.


Author
------
Jeff Buck

The original version of Handmade Hero is being created by Casey Muratori.

You can find more information about Handmade Hero at:
	http://handmadehero.org


