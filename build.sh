#!/bin/sh

rm -rf build/
mkdir -p build && cd build/

meson .. -Dprefix=/usr
ninja

# Once builded the executable can be found in the build directory