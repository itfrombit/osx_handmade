OS X Handmade Hero
==================

This is an ongoing OS X port of Casey Muratori's Handmade Hero.

This repository works with Casey's source code from handmade_hero_161.

This version uses native OS X libraries for the platform layer:
- CoreAudio for sound
- OpenGL for graphics
- IOKit's HID for joystick input

The goal is to be able to drop in Casey's platform independent
game source code and compile and run it unchanged.

This Xcode project first builds a dynamic library subproject
called libhandmade.dylib and includes it in the Handmade Hero
product directory of the application bundle.


Note 2015-10-13:
----------------
I'm currently bringing the Mac port up-to-date after a summer hiatus.
This version is compatible with Day 161 (after the block allocator,
before fonts).

Note that this version might suffer from an audio glitch that Casey has not
yet debugged. I believe the OS X Core Audio streaming is working properly.


IMPORTANT
---------

Once you clone or update this repository, copy over Casey's .cpp
and .h source files to the root directory of this repository.

Also, copy over the test, test2, and test3 asset folders to the
root directory of this repository.

Before you build the application for the first time, you need to
create the packed asset files. To do this, open the separate test_asset_builder
Xcode project and build and run it once. It should find the test 
asset directories and create the *.hha files. You only need to
repeat this step when Casey changes the .hha file format, or when he
modifies or adds new assets.


EXTRA IMPORTANT: For better rendering performance, build the project in Release mode.
The Debug mode version may drop audio frames.

You can also set the renderAtHalfSpeed flag in HandmadeView.mm to
reduce the effective rendering rate to 30fps instead of the default
60fps. The proper Core Audio sound buffer size is automatically adjusted.

I've implemented the necessary calls to output the Debug Cycle Counters.
To enable this, just replace the three empty stub #defines in handmade_platform.h
at lines 189-191 with the ones just above inside the _MSC_VER check.


Author
------
Jeff Buck

The original version of Handmade Hero is being created by Casey Muratori.

You can find more information about Handmade Hero at:
	http://handmadehero.org


