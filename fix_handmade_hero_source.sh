#!/bin/sh

ex cpp/code/handmade_debug_interface.h <<EOF
:%s/#define TIMED_FUNCTION(...) TIMED_BLOCK_(DEBUG_NAME(__FUNCTION__), ## __VA_ARGS__)/#define TIMED_FUNCTION(...) TIMED_BLOCK_(DEBUG_NAME(__FILE__), __COUNTER__)/g
:%s/__FUNCTION__/__FILE__/g
:wq
EOF
