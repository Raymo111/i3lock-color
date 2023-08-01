#!/bin/sh

LICENSE='/usr/share/licenses/i3lock-color/LICENSE'

./build.sh
cd build
sudo ninja install
sudo install -Dm644 ../LICENSE "$LICENSE"

echo "i3lock-color installed. The binary and manpage listing are \`i3lock'.
The license can be found at $LICENSE
GitHub repo: https://github.com/Raymo111/i3lock-color
Discord server: https://discord.gg/FzVPghyDt2"

# To uninstall just cd into the build direcory and execute:
# sudo ninja uninstall