#!/bin/bash
./build.sh

cd build

sudo make install

sudo install -Dm644 ../LICENSE "/usr/share/licenses/i3lock-color/LICENSE"
