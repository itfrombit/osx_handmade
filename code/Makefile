CXX	= clang

HANDMADE_CODE_PATH = ../cpp/code
HANDMADE_SOURCES_PATH = ../sources
HANDMADE_TAGS_PATH = ../tags
HANDMADE_ART_PATH = ../art
HANDMADE_SOUND_PATH = ../sound

HANDMADE_FLAGS = -DHANDMADE_USE_VSYNC=0 -DHANDMADE_PROFILE=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_OSX=1

HANDMADE_COMPILER_FLAGS = -fno-exceptions -fno-rtti

HANDMADE_WARNING_FLAGS = -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable -Wno-c++11-narrowing -Wno-missing-braces -Wno-logical-not-parentheses -Wno-switch -Wno-write-strings -Wno-c++11-compat-deprecated-writable-strings -Wno-tautological-compare -Wno-missing-braces -Wno-null-dereference -Wno-writable-strings


COPTS = -g -Wall -fno-inline $(HANDMADE_FLAGS)
COPTS = -O3 -Wall $(HANDMADE_FLAGS)


CPP11_FLAGS	= -std=c++11 -stdlib=libc++ -ggdb
CPP11_LD_FLAGS = -lstdc++


OSX_LD_FLAGS = -framework Cocoa -framework IOKit -framework AudioUnit


DYNAMIC_COMPILE_PATH=$(shell pwd)
DYNAMIC_COMPILE_COMMAND=$(DYNAMIC_COMPILE_PATH)/dynamic_compile.sh


#default:	osx_hhfont
default: clean libhandmade.dylib handmade package

# 'quick' only rebuilds the local debugging 'handmade' executable. Does not update the Handmade.app bundle!
quick: quick_clean libhandmade.dylib libhandmade_opengl.dylib handmade
	cp handmade ./Contents/MacOS/Handmade
	cp libhandmade.dylib ./Contents/MacOS/libhandmade.dylib
	cp libhandmade_opengl.dylib ./Contents/Resources/libhandmade_opengl.dylib
	cp handmade ./Handmade.app/Contents/MacOS/Handmade
	cp libhandmade.dylib ./Handmade.app/Contents/MacOS/libhandmade.dylib
	cp libhandmade_opengl.dylib ./Handmade.app/Contents/Resources/libhandmade_opengl.dylib

BINARIES = libhandmade.dylib libhandmade_opengl.dylib handmade osx_asset_builder test_png hhaedit HandmadeTabView HandmadeRendererTest

all: clean libhandmade.dylib handmade package osx_asset_builder test_png hhaedit HandmadeTabView HandmadeRendererTest

package:
	# The 'local' bundle for convenient debugging with lldb
	mkdir -p ./Contents/MacOS
	mkdir -p ./Contents/Resources
	mkdir -p ./Contents/Resources/data
	cp -R ../tags ./Contents/Resources
	cp -R ../sources ./Contents/Resources
	# Uncomment the following 4 lines to use our own generated fonts
	# cp osx_fonts/liberation_mono* ./Contents/Resources/sources/base_game/fonts
	# mkdir -p ./Contents/Resources/tags/fonts
	# cp LiberationMono.hht ./Contents/Resources/tags/fonts/
	#cp Monaco.hht ./Contents/Resources/tags/fonts/
	#
	#cp $(HANDMADE_ASSETS_PATH)/testfonts_v2.hha ./Contents/Resources/data/testfonts_v2.hha
	#cp $(HANDMADE_ASSETS_PATH)/intro_art_v2.hha ./Contents/Resources/data/intro_art_v2.hha
	#cp $(HANDMADE_ART_PATH)/*.png ./Contents/Resources/art
	cp libhandmade.dylib ./Contents/MacOS/libhandmade.dylib
	cp libhandmade_opengl.dylib ./Contents/Resources
	# The "official" app bundle
	#rm -rf ../build/Handmade.app
	mkdir Handmade.app
	cp -R ./Contents ./Handmade.app/
	cp handmade ./Handmade.app/Contents/MacOS/Handmade
	#mv Handmade.app ../build


osx_asset_builder:	osx_asset_builder.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) -o $@ $^

test_png:	$(HANDMADE_CODE_PATH)/test_png.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) -o $@ $^

hhaedit:	$(HANDMADE_CODE_PATH)/hhaedit.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) -o $@ $^

hhsphere:	$(HANDMADE_CODE_PATH)/hhsphere.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) -o $@ $^

osx_hhfont:	osx_hhfont.cpp
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) -framework Foundation -framework CoreGraphics -framework CoreText -o $@ $^

#handmade_optimized.o:	$(HANDMADE_CODE_PATH)/handmade_optimized.cpp
#	$(CXX) -O3 -Wall $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_FLAGS) $(HANDMADE_WARNING_FLAGS) $(CPP11_FLAGS) -c $<

libhandmade.dylib: $(HANDMADE_CODE_PATH)/handmade.cpp $(HANDMADE_CODE_PATH)/handmade.h $(HANDMADE_CODE_PATH)/handmade_dev_ui.cpp
	$(CXX) $(COPTS) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) $(CPP11_FLAGS) -dynamiclib -o $@ $(HANDMADE_CODE_PATH)/handmade.cpp


libhandmade_opengl.dylib: osx_handmade_opengl.mm osx_handmade_opengl.cpp osx_handmade_renderer.h
	$(CXX) $(COPTS) $(HANDMADE_COMPILER_FLAGS) $(HANDMADE_WARNING_FLAGS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) -dynamiclib -framework Cocoa -framework QuartzCore -framework OpenGL -o $@ osx_handmade_opengl.mm


handmade: osx_main.mm osx_handmade.h $(HANDMADE_CODE_PATH)/handmade_platform.h libhandmade_opengl.dylib
	$(CXX) $(COPTS) $(CPP11_FLAGS) -I$(HANDMADE_CODE_PATH) $(HANDMADE_COMPILER_FLAGS) -Wno-writable-strings -Wno-deprecated-declarations -Wno-null-dereference -Wno-c++11-compat-deprecated-writable-strings -Wno-unused-function -Wno-unused-variable -Wno-missing-braces $(CPP11_LD_FLAGS) $(OSX_LD_FLAGS) -o $@ osx_main.mm


HandmadeTabView:	osx_tabview.mm
	$(CXX) $(COPTS) $(HANDMADE_COMPILER_FLAGS) -framework Cocoa $(CPP11_LD_FLAGS) -o $@ $^
	rm -rf ./HandmadeTabView.app
	mkdir -p ./HandmadeTabView.app/Contents/MacOS
	cp HandmadeTabView HandmadeTabView.app/Contents/MacOS/HandmadeTabView
	cp osx_tabview.info.plist HandmadeTabView.app/Contents/Info.plist

HandmadeRendererTest:	osx_renderer_test.mm libhandmade_opengl.dylib
	$(CXX) $(COPTS) $(HANDMADE_COMPILER_FLAGS) -I$(HANDMADE_CODE_PATH) $(CPP11_FLAGS) -Wno-null-dereference -Wno-missing-braces -Wno-unused-variable -Wno-unused-function -Wno-writable-strings -framework Cocoa $(CPP11_LD_FLAGS) -o $@ $<
	rm -rf ./HandmadeRendererTest.app
	mkdir -p ./HandmadeRendererTest.app/Contents/MacOS
	mkdir -p ./HandmadeRendererTest.app/Contents/Resources
	cp $(HANDMADE_SOURCES_PATH)/renderer_test/test_cube_grass.bmp ./HandmadeRendererTest.app/Contents/Resources
	cp $(HANDMADE_SOURCES_PATH)/renderer_test/test_cube_wall.bmp ./HandmadeRendererTest.app/Contents/Resources
	cp $(HANDMADE_SOURCES_PATH)/renderer_test/test_sprite_tree.bmp ./HandmadeRendererTest.app/Contents/Resources
	cp $(HANDMADE_SOURCES_PATH)/renderer_test/test_sprite_head.bmp ./HandmadeRendererTest.app/Contents/Resources
	cp $(HANDMADE_SOURCES_PATH)/renderer_test/test_cover_grass.bmp ./HandmadeRendererTest.app/Contents/Resources
	mv HandmadeRendererTest HandmadeRendererTest.app/Contents/MacOS/HandmadeRendererTest
	cp libhandmade_opengl.dylib HandmadeRendererTest.app/Contents/Resources
	#cp osx_tabview.info.plist HandmadeTabView.app/Contents/Info.plist

quick_clean:
	rm -f handmade libhandmade.dylib libhandmade_opengl.dylib

clean:
	rm -rf *.o *.dylib $(BINARIES) Contents Handmade.app HandmadeTabView.app HandmadeRendererTest.app osx_hhfont *.dSYM

