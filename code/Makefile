CXX			= clang

HANDMADE_CODE_PATH = ../cpp/code
HANDMADE_ASSETS_PATH = ../data

HANDMADE_FLAGS = -DHANDMADE_PROFILE=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_OSX=1 -DHANDMADE_MIN_OSX

HANDMADE_COMPILER_FLAGS = -fno-exceptions -fno-rtti
HANDMADE_WARNING_FLAGS = -Wno-unused-function -Wno-unused-variable -Wno-c++11-narrowing -Wno-missing-braces -Wno-logical-not-parentheses -Wno-switch -Wno-write-strings -Wno-c++11-compat-deprecated-writable-strings -Wno-tautological-compare -Wno-missing-braces -Wno-null-dereference -Wno-writable-strings


# Use the following to force compiling with 10.7 SDK testing:
#HANDMADE_FLAGS = -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_OSX=1 -DHANDMADE_MIN_OSX -Wno-null-dereference -isysroot /Applications/Xcode-Beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk

COPTS = -g -Wall -fno-inline $(HANDMADE_FLAGS)
COPTS = -O3 -Wall $(HANDMADE_FLAGS)


CPP11_FLAGS	= -std=c++11 -stdlib=libc++ -ggdb
CPP11_LD_FLAGS = -lstdc++

OSX_FLAGS =
OSX_LD_FLAGS = -framework Cocoa -framework QuartzCore -framework OpenGL -framework IOKit -framework AudioUnit
OSX_CT_LD_FLAGS = -framework CoreText -framework CoreFoundation
#OSX_LD_FLAGS = -framework Cocoa -framework QuartzCore -framework OpenGL -framework IOKit -framework AudioUnit -Wl,-syslibroot /Applications/Xcode-Beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk

BINARIES = handmade libhandmade.dylib osx_asset_builder

DYNAMIC_COMPILE_PATH=$(shell pwd)
DYNAMIC_COMPILE_COMMAND=$(DYNAMIC_COMPILE_PATH)/dynamic_compile.sh

#default: clean libhandmade.dylib handmade
default: clean libhandmade.dylib handmade package

quick: libhandmade.dylib handmade

all: clean libhandmade.dylib handmade package osx_asset_builder handmade_png

handmade:	osx_main.o osx_handmade.o
	$(CXX) $(COPTS) $(OSX_LD_FLAGS) $(CPP11_LD_FLAGS) -o $@ $^

package:
	rm -rf ../build/Handmade.app
	rm -rf ./Contents/Resources
	mkdir -p Handmade.app/Contents/MacOS
	mkdir -p Handmade.app/Contents/Resources
	mkdir -p Handmade.app/Contents/code
	cp handmade Handmade.app/Contents/MacOS/Handmade
	cp $(HANDMADE_ASSETS_PATH)/test1.hha Handmade.app/Contents/Resources/test1.hha
	cp $(HANDMADE_ASSETS_PATH)/test2.hha Handmade.app/Contents/Resources/test2.hha
	cp $(HANDMADE_ASSETS_PATH)/test3.hha Handmade.app/Contents/Resources/test3.hha
	cp $(HANDMADE_ASSETS_PATH)/testfonts.hha Handmade.app/Contents/Resources/testfonts.hha
	cp $(HANDMADE_ASSETS_PATH)/intro_art.hha Handmade.app/Contents/Resources/intro_art.hha
	cp libhandmade.dylib Handmade.app/Contents/MacOS/libhandmade.dylib
	mv Handmade.app ../build/
	# The 'local' bundle for convenient debugging with lldb
	mkdir -p ./Contents/MacOS
	mkdir -p ./Contents/Resources
	mkdir -p ./Contents/code
	cp $(HANDMADE_ASSETS_PATH)/test1.hha ./Contents/Resources/test1.hha
	cp $(HANDMADE_ASSETS_PATH)/test2.hha ./Contents/Resources/test2.hha
	cp $(HANDMADE_ASSETS_PATH)/test3.hha ./Contents/Resources/test3.hha
	cp $(HANDMADE_ASSETS_PATH)/testfonts.hha ./Contents/Resources/testfonts.hha
	cp $(HANDMADE_ASSETS_PATH)/intro_art.hha ./Contents/Resources/intro_art.hha
	cp libhandmade.dylib ./Contents/MacOS/libhandmade.dylib

osx_asset_builder:	osx_asset_builder.o
	$(CXX) $(COPTS) $(OSX_CT_LD_FLAGS) -o $@ $^

osx_asset_builder.o:	osx_asset_builder.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_WARNING_FLAGS) -c $<

handmade_png:	$(HANDMADE_CODE_PATH)/handmade_png.o
	$(CXX) $(COPTS) $(OSX_CT_LD_FLAGS) -o $@ $^

handmade_png.o:	$(HANDMADE_CODE_PATH)/handmade_png.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_WARNING_FLAGS) -c $<


#libhandmade.dylib: handmade.o handmade_optimized.o
libhandmade.dylib: handmade.o
	$(CXX) $(COPTS) -lstdc++ -dynamiclib -o $@ $^


#handmade_optimized.o:	$(HANDMADE_CODE_PATH)/handmade_optimized.cpp
#	$(CXX) -O3 -Wall $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_FLAGS) $(HANDMADE_WARNING_FLAGS) $(CPP11_FLAGS) -c $<

handmade.o: $(HANDMADE_CODE_PATH)/handmade.cpp $(HANDMADE_CODE_PATH)/handmade.h
	$(CXX) $(COPTS) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) $(CPP11_FLAGS) -c $<


osx_handmade.o: osx_handmade.cpp osx_handmade.h osx_handmade_debug.cpp osx_handmade_file.cpp osx_handmade_process.cpp osx_handmade_thread.cpp osx_handmade_audio.cpp osx_handmade_hid.cpp osx_handmade_dylib.cpp osx_handmade_playback.cpp osx_handmade_game.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) -Wno-writable-strings -Wno-deprecated-declarations -Wno-null-dereference -Wno-c++11-compat-deprecated-writable-strings -Wno-unused-function -Wno-unused-variable -Wno-missing-braces -c $<

osx_main.o: osx_main.mm osx_handmade.cpp osx_handmade.h $(HANDMADE_CODE_PATH)/handmade.h $(HANDMADE_CODE_PATH)/handmade.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) -Wno-writable-strings -Wno-deprecated-declarations -Wno-null-dereference -Wno-c++11-compat-deprecated-writable-strings -Wno-unused-function -Wno-unused-variable -c $<

clean:
	rm -rf *.o $(BINARIES) Handmade.app

