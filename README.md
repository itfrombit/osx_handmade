osx_handmade
============

A port of Handmade Hero (http://handmadehero.org) for macOS.

This repository works with Casey's source code from Day 576.

If you are compiling for the first time, you might want to
skip down to the [Compiling and Running](#compiling-and-running)
section first.

This macOS platform layer code does not need to be updated for
every episode of Handmade Hero, although I do test each day's code
on macOS. If you see "missing" days in this macOS repository, it just
means that the most recent version of the macOS platform layer will
work. For example, day 405 of the macOS platform layer
will work with Casey's Handmade Hero days 405 through 409.


# Recent Update Notes

## Day 576 Notes:

Shaders now get recompiled upon a hot-reload. If you know you are
only changing the renderer (and not the game code), you can just
`make libhandmade_opengl.dylib` to quickly rebuild just the
renderer code.


## Day 575 Note:

Hot loading of the renderer is now supported.

The default make target is now `make quick`, which recompiles
all of the code, but does not rebuild the application bundle.
This will prevent re-importing the asset libraries on startup.

If you want to rebuild the application bundles and force a
re-import of the asset libraries, you now run `make full`.


## Day 552 Note:

Somewhere between Day 551 and 552, the PushX macros were fixed
so I removed that patch. However, we are zero sum on patches
for the day as I had to add a new patch to fix two misplaced
`alignas` decorators in handmade_sampling_spheres.inl.

As usual, you can apply patches by running:

    sh fix_handmade_hero_source.sh


## Day 541 Note:

I added a new patch to fix a few compile errors in the PushX
macros madness in handmade_memory.h. I'm not sure how the Visual
Studio compiler let those slide, but clang said no way.

You can apply the patch by running:

    sh fix_handmade_hero_source.sh


## Day 532 Note:

I wrote a native macOS version of hhfont using Core Text. It is not
required, as you can just use the font images and hht file that Casey
generated. I was mostly interested in learning about some of the
details of Core Text that I hadn't used before.

If you do want to generate your own font images and hht files,
you can compile and run the utility like this:

    make osx_hhfont

If you run `osx_hhfont` with no command line options, you will see
some usage details. Once you generate the .hht file and the png
images, copy the .hht file and the .png files into the appropriate
tags and sources directories.

`osx_hhfont` also has a handy debugging mode to ASCII print a single
code point or glyph. See the usage for details.

## Day 526 Note:

Make sure you reimport your assets (or just do a full 'make' build)
so you can hear the sound and music play.


## Day 516 Note:

Make sure you move the /sources directory tree to the root directory
of this repository before running make.

A full build triggers an asset import when running the game for the
first time. Asset importing is slow. If you are just making code
changes and don't need to reimport assets between builds, then
'make quick' is the target you want.


## Day 498 Note:

After you do a clean compile and run the game for the first time,
you have to import and save the textures using the new DevUI
command button (hit F10 to display the DevUI, F9 to dismiss it when
finished).


## Day 493 Note:

The handmade_dev_ui.cpp compile error was fixed in day 493. You should still
run 'fix_handmade_hero_source.sh' to patch the lingering OpenGL
artifact issue if you notice it happening on your particular
graphics hardware.

Compilation note: Hot reloading works when you use the
'make quick' target. This will recompile the executable and the
dynamic library and move them into place without needing to
rebuild the entire application bundle.

The 'make quick' target is also useful to prevent the assets from
being re-imported when rebuilding the application bundle, which
is what happens when performing a default 'make'.


## Day 492 Note:

I added a new patch to fix a compile error in handmade_dev_ui.cpp. clang
complains about passing string literals to the Button() function which
expects a char pointer. For now, the patch just casts the string literals to
character pointers.

You can apply the patch by running:

    sh fix_handmade_hero_source.sh

Also, don't forget to regenerate your .hha files to fix the font glyph alignment
bug.


## Day 472 Note:

If you want to run the HandmadeRendererTest with textures, you will
have to recreate the sample textures that Casey created on stream
and put them in the ./data/renderer_test directory. I recreated
them with Gimp on macOS, but I didn't want to redistribute Casey's
original artwork.

WARNING: The popular native graphics editing packages on macOS
(Acorn, Pixelmator, Affinity Photo, Preview, etc.) either do not
export bitmaps at all, or do not write bitmaps in the
.bmp format that the Handmade Hero code expects. The y-axis is
flipped and a different compression method is used. I'd recommend
downloading and using Gimp to create the .bmp files.


## Day 471 Note:

I added an macOS version of the HandmadeRendererTest application.
You can build this by running:

    make HandmadeRendererTest

in the code subdirectory. This will create the HandmadeRendererTest.app
bundle. You can run the application from the Finder or by launching it
from a shell prompt like this:

    open HandmadeRendererTest.app


## Day 467 Note:

Day 467 started using version 1 assets, so don't forget to
rewrite the .hha files! You can rewrite an .hha file like this:

    hhaedit -rewrite ../data/intro_art.hha ../data/intro_art_v1.hha

Day 468 started using local.hha to dynamically import .png resources
at runtime. Make sure you create a local.hha file before compiling
the application bundle if you want this functionality at runtime.

    hhaedit -create ../data/local.hha


## Day 466 Note:

I added an macOS version of the TabView utility. You can build this
by running:

    make HandmadeTabView

in the code subdirectory (the same directory where you build Handmade Hero).
This will create the HandmadeTabView.app bundle.

Some notes:

- Create .hha dump files for viewing like this:

      hhaedit -dump ../data/intro_art_v1.hha > intro_art_v1.hha.dump

- Run HandmadeTabView.app. Either double-click the .app file in the Finder,
  or run it from a shell prompt like this:

      open HandmadeTabView.app

- The application launches with an empty document. You can drag and drop
a dump file from the Finder onto any existing window (empty or not) and it
will replace the current contents with the contents of the dropped file.
You can also use the standard Open File or Open Recent menu items to
select a file to open.

- The application supports multiple files open at the same time so that
you can compare dump file contents.

- 'Command +' and 'Command -' (also available from the View menu) will
expand/collapse all nodes of a dump tree in the currently active window.

- 'Command r' will reload the file contents of the currently active window.


## 2018-08-17 Note:

While the code compiles as-is on macOS, you must first apply a patch to
Casey's Handmade Hero source code to get the game to render properly. You can
apply the patch by running:

    sh fix_handmade_hero_source.sh

This patches a problem in handmade_opengl.cpp:

- Casey's final implementation for multisample depth peeling on Day 388
causes undesirable artifacts on macOS. Instead of averaging the Min and Max
Depth values, I reverted to using the MaxDepth value in the shader with a
threshold of 0.02. This still produces some minor artifacts, but it is a big
improvement over the averaging method. I'll continue to look into this.


Note on using joysticks: Casey added a Clutch control on Day 443. He
uses an XBox controller's Left or Right "Trigger" buttons for the Clutch
control. I currently have this mapped to HID Button 6 on macOS, which on a
Logitech Dual Action controller, corresponds to the top right shoulder
button. Your mileage may vary with other controllers. Let me know if you
have problems.


# Compiling and Running

Once you clone or update this repository, copy (or clone, if you are
using Casey's Github repository) Casey's .cpp
and .h source files to the cpp/code subdirectory of this repository.

Also, copy over the sources and tags asset folders.

Your directory structure should look like this:

    .                      // root directory of this repository
	code/Makefile          // and the rest of the macOS platform code
	code/osx_handmade.cpp  // and the rest of the macOS platform code
	cpp/code/handmade.cpp  // and the rest of Casey's code
	patches/               // patches to Casey's HH source code
	sources/               // artwork and sounds
	tags/                  // descriptions and instructions for asset import
	xcode/                 // Xcode project files


The first time you compile, run 'make full' from the code
directory (Note: not the cpp/code directory!) to build the application bundle.
Note that a 'make full' will cause an entire asset import on the subsequent
game startup, which is slow. And it's extra slow in debug mode, so doing
the asset import in release mode is recommended.

Once you have done a full build and have created the application
bundles, you can run 'make' to just recompile the dynamic libraries and
the executable (the code parts). Most of the build time when running a 'make full'
is spent copying over the large asset files, so 'make quick' avoids that step
and also avoids a full asset import on game startup.

You can then either run 'handmade' directly, or 'open Handmade.app'.
The advantage of running 'handmade' directly is that debug console output
(printf's, etc.) will be displayed in your terminal window instead
of being logged to the System Console.

I typically run the game from the command line like this:

	./handmade

I also use lldb to debug. lldb has a 'gui' command that will
display a source code and variable view while debugging. It's not great,
but it's sometimes better than using the plain command line mode in lldb.
You can also debug using Xcode.

Hot-loading is supported, so you can just run 'make' again (or have your
favorite editor do it) while the application is running to build and
reload the newest code.

For better rendering performance using the software renderer, build
the project in Release mode. You can also set the renderAtHalfSpeed
flag in HandmadeView.mm to reduce the effective rendering rate to 30fps
instead of the default 60fps. The proper Core Audio sound buffer size
is automatically adjusted.


# Author

Jeff Buck

The original version of Handmade Hero is being created by Casey Muratori.

