#!/bin/sh

patch cpp/code/handmade_opengl.cpp -i patches/handmade_opengl.cpp.day430.patch
patch cpp/code/handmade_lighting.cpp -i patches/handmade_lighting.cpp.day434.patch
patch cpp/code/handmade_simd.h -i patches/handmade_simd.h.day434.patch

