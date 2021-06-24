#!/bin/bash
./build.sh

cd build

sudo make install

sudo install -Dm644 ../LICENSE "/usr/share/licenses/i3lock-color/LICENSE"

if [[ -f /usr/bin/zsh ]]; then
  sudo cp ../_i3lock /usr/share/zsh/vendor-completions
fi

if [[ -f /usr/bin/bash ]]; then
  sudo cp ../i3lock-bash /usr/share/bash-completion/completions/i3lock
fi
