osx_handmade
============

A port of Handmade Hero (http://handmadehero.org) for OS X.

This repository works with Casey's source code from handmade_hero_day_301.


Note 2016-07-11:
----------------
I am currently in the process of bringing the OS X platform layer up-to-date
after another long winter/spring hiatus.


Note 2016-08-28:
----------------
This version is compatible with Day 301.

However, the Handmade Hero source code is currently using the
non-portable _snprintf_s function in handmade_debug.cpp.
To fix the source code, run

    sh fix_handmade_hero_source.sh

after copying over Casey's source code, but before running 'make'
for the first time.

If you would rather fix the error by hand instead of running the above
shell script, just insert the contents of the provided file 'vsprintf.cpp'
near the top of the handmade_debug.cpp (just below the '#include <stdio.h>' line).

The TIMED_FUNCTION() macro in handmade_debug_interface.h does not compile
under clang. I've hacked a temporary fix for this, but hopefully Casey
will address this in a not-too-distant episode.

Some things have changed from earlier versions. The directory
structure has been cleaned up and is now more compatible with
the Handmade Hero Github repository.

The architecture of the platform layer has also changed. CVDisplayLink
is no longer used. Instead, OpenGL vsync is used to update the frame.
Also, a new manual run loop was implemented to be more compatible with
the Windows platform layer.

The platform layer no longer creates an explicit NSOpenGLView-derived object.
This object was just extra boilerplate code. We now just create an OpenGLContext
and assign it to the application's default contentView.

Finally, I combined the two OS X Handmade Hero projects that I had been
maintaining. I originally created two projects to show how to write an OS X
program with and without Xcode. However, most of the code between the
two projects was identical and there was no real reason to continue
as two separate projects. This combined repository allows you to develop
either way. If you wwant to use Xcode, open the Handmade Hero Xcode
project in the xcode subdirectory and build it as before. If you do not
want to use Xcode, run 'make' from the code subdirectory and it should
also work as before.


Compiling and Running
---------------------

Once you clone or update this repository, copy (or clone, if you are
using Casey's Github repository) Casey's .cpp
and .h source files to the cpp/code subdirectory of this repository.

Also, copy over the test, test2, and test3 asset folders, and the 
intro_art.hha file to the data subdirectory of this repository.

Before you build the application for the first time, you need to
create the packed asset files. To do this, run

    make osx_asset_builder

and then execute the osx_asset_builder command line program. This will
create the .hha files. From then on, you can just run 'make' from the code
directory (Note: not the cpp/code directory!) to build the application bundle.

If you already have packed asset files, you can just copy them to the data
subdirectory and skip building and running the osx_asset_builder.

You can then either run 'handmade' directly, or 'open Handmade.app'.
The advantage of running 'handmade' directly is that debug console output 
(printf's, etc.) will be displayed in your terminal window instead
of being logged to the System Console.

Hot-loading is supported, so you can just run 'make' again (or have your
favorite editor do it) while the application is running to build and
reload the newest code.

For better rendering performance using the software renderer, build
the project in Release mode. You can also set the renderAtHalfSpeed
flag in HandmadeView.mm to reduce the effective rendering rate to 30fps
instead of the default 60fps. The proper Core Audio sound buffer size
is automatically adjusted.


Author
------
Jeff Buck

The original version of Handmade Hero is being created by Casey Muratori.

