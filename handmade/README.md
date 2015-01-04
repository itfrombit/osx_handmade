This directory contains the Xcode project to build the
platform-independent game code dynamic library. The
library is called libhandmade.dylib and is copied
to the Handmade Hero application bundle's executable
directory (Handmade Hero.app/Contents/MacOS). You
can replace this dynamic library at runtime for hot
reloading while debugging the game.

To build this project, you need to copy the appropriate
versions of the game code from Casey's
code snapshots to this directory. The top level
README.md file contains information on the specific
files to copy as well as which version of
Casey's code (e.g. Day 035) is compatible with this
OS X port.



