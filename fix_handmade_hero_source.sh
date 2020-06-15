#!/bin/sh

patch cpp/code/handmade_renderer_opengl.cpp -i patches/handmade_renderer_opengl.cpp.day475.patch
patch cpp/code/handmade_sampling_spheres.inl -i patches/handmade_sampling_spheres.inl.day552.patch

