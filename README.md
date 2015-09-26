OS X Handmade Hero
==================

This is an ongoing OS X port of Casey Muratori's Handmade Hero.

This version uses native OS X libraries for the platform layer:
- CoreAudio for sound
- OpenGL for graphics
- IOKit's HID for joystick input

The goal is to be able to drop in Casey's platform independent
game source code and compile and run it unchanged.

This Xcode project first builds a dynamic library subproject
called libhandmade.dylib and includes it in the Handmade Hero
product directory of the application bundle.


Note 2015-09-26:
----------------
I'm currently bringing the Mac port up-to-date after a summer hiatus.
This version is compatible with Day 121. Threading is up next.


IMPORTANT
---------

Once you clone or update from this repository, copy over the
following files from Casey's source code to the handmade
subdirectory:
- handmade.cpp
- handmade.h
- handmade_entity.cpp
- handmade_entity.h
- handmade_intrinsics.h
- handmade_math.h
- handmade_platform.h
- handmade_random.h
- handmade_render_group.cpp
- handmade_render_group.h
- handmade_sim_region.cpp
- handmade_sim_region.h
- handmade_world.cpp
- handmade_world.h

Also, copy over the test and test2 bitmap image asset folders to the
root directory of this repository.

This repository works with Casey's source code from
handmade_hero_121.

EXTRA IMPORTANT: For better rendering performance, build the project in Release mode.

You can also temporarily set the renderAtHalfSpeed flag in HandmadeView.mm to
reduce the effective rendering rate to 30fps instead of the default
60fps. This is temporarily now the default until the renderer is optimized.


I've implemented the necessary calls to output the Debug Cycle Counters.
To enable this, just replace the three empty stub #defines in handmade_platform.h
at lines 170-172 with the ones just above inside the _MSC_VER check.


Author
------
Jeff Buck

The original version of Handmade Hero is being created by Casey Muratori.

You can find more information about Handmade Hero at:
	http://handmadehero.org


