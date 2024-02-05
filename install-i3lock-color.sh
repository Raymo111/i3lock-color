#!/bin/bash

LICENSE='/usr/share/licenses/i3lock-color/LICENSE'
SUDO='sudo'

[ "$UID" -eq 0 ] && unset SUDO

./build.sh
cd build
$SUDO make install
$SUDO install -Dm644 ../LICENSE "$LICENSE"

echo "i3lock-color installed. The binary and manpage listing are \`i3lock'.
The license can be found at $LICENSE
GitHub repo: https://github.com/Raymo111/i3lock-color
Discord server: https://discord.gg/FzVPghyDt2"
