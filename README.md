osx_handmade
============

A port of Handmade Hero (http://handmadehero.org) for OS X.

This repository works with Casey's source code from Day 395.


2018-08-02 Note:
----------------

While the code compiles as-is on OS X, you must first apply a patch to
Casey's Handmade Hero source code to get the game to run properly. You can
apply the patch by running:

    sh fix_handmade_hero_source.sh

This patches several problems in handmade_opengl.cpp:

1. Mac OS X supports an OpenGL 3.2 core profile, but not an OpenGL 3.0 core
profile. The OpenGL C code works okay as-is, but the Handmde Hero GLSL shader
language is set to 1.3 ("130"), which Mac OS X refuses to compile under an
OpenGL 3.2 core profile. For now, simply changing the GLSL version to
1.5 ("150") fixes the issue.

2. Casey's final implementation for multisample depth peeling on Day 388
causes undesirable artifacts on Mac OS X. Instead of averaging the Min and Max
Depth values, I reverted to using the MaxDepth value in the shader with a
threshold of 0.02. This still produces some minor artifacts, but it is a big
improvement over the averaging method. I'll continue to look into this.

3. gl_FragData is deprecated in the version of GLSL that the Mac supports.
You must specify this array as an out variable.


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
create the .hha files.

If you already have packed asset files, you can just copy them to the data
subdirectory and skip building and running the osx_asset_builder.

From then on, you can just run 'make' from the code
directory (Note: not the cpp/code directory!) to build the application bundle.

Once you have done a full build and have created the application
bundles, you can run 'make quick' to just recompile the dynamic library and
the executable. Most of the build time when running the default 'make'
is spent copying over the large asset files, so 'make quick' avoids that step.

You can then either run 'handmade' directly, or 'open Handmade.app'.
The advantage of running 'handmade' directly is that debug console output
(printf's, etc.) will be displayed in your terminal window instead
of being logged to the System Console.

I typically run the game from the command line like this:

	./handmade

I also use lldb to debug. lldb has a 'gui' command that will
display a source code and variable view while debugging. It's not great,
but it's almost always better than using Xcode, and is sometimes better than
using the plain command line mode in lldb.

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

