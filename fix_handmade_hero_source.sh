#!/bin/sh

patch cpp/code/handmade_renderer_opengl.cpp -i patches/handmade_renderer_opengl.cpp.day475.patch
patch cpp/code/handmade_memory.h -i patches/handmade_memory.h.day541.patch

