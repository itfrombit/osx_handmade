#!/bin/sh
ex handmade_platform.h <<EOF
:501,501s/debug_table \*//
:wq
EOF

ex handmade_generated.h <<EOF2
:1,60s/(u32)/(u64)/g
:wq
EOF2

ex handmade_debug.cpp +11 <<EOF3
:r vsprintf.cpp
:749,749s/(u32)/(u64)/g
:wq
EOF3
