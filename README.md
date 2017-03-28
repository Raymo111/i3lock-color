i3lock-color - improved screen locker
===============================

_This is just a re-patched version of i3lock with the commits from [i3lock-color](https://github.com/eBrnd/i3lock-color); all the credit for the color functionality goes to [eBrnd](https://github.com/eBrnd/) !_

i3lock is a simple screen locker like slock. After starting it, you will
see a white screen (you can configure the color/an image). You can return
to your screen by entering your password.

Many little improvements have been made to i3lock over time:

- i3lock forks, so you can combine it with an alias to suspend to RAM
  (run "i3lock && echo mem > /sys/power/state" to get a locked screen
   after waking up your computer from suspend to RAM)

- You can specify either a background color or a PNG image which will be
  displayed while your screen is locked.

  -  You can also specify additional color options with the following command-line options:
     - `--insidevercolor=rrggbbaa` -- Inside of the circle while the password is being verified
     - `--insidewrongcolor=rrggbbaa` -- Inside of the circle when a wrong password was entered
     - `--insidecolor=rrggbbaa` -- Inside of the circle while typing/idle
     - `--ringvercolor=rrggbbaa` -- Outer ring while the password is being verified
     - `--ringwrongcolor=rrggbbaa` -- Outer ring when a wrong password was entered
     - `--ringcolor=rrggbbaa` -- Outer ring while typing/idle
     - `--linecolor=rrggbbaa` -- Line separating outer ring from inside of the circle
     - `--separatorcolor=rrggbbaa` -- Lines delimiting the highlight segments
     - `--textcolor=rrggbbaa` -- Text ("verifying", "wrong!")
     - `--keyhlcolor=rrggbbaa` -- Keypress highlight segments
     - `--bshlcolor=rrggbbaa` -- Backspace highlight segments
     - `--line-uses-ring`, `-r` -- the line between the inside and outer ring uses the ring color for its color
     - `--line-uses-inside`, `-s` -- the line between the inside and outer ring uses the inside color for its color
     - `-S, --screen` -- specifies which display to draw the unlock indicator on
     - `-k, --clock` -- enables the clock display.
     - `-B, --blur` -- enables Gaussian blur
     - `--timestr="%H:%M:%S"` -- allows custom overriding of the time format string. Accepts `strftime` formatting. Default is `"%H:%M:%S"`.
     - `--datestr="%A, %m %Y"` -- allows custom overriding of the date format string. Accepts `strftime` formatting. Default is `"%A, %m %Y"`.
  - All the colors have an alpha channel now. Please keep in mind that this was not intended when the program was originally written, so making things transparent that weren't before can make it look strange.

- You can specify whether i3lock should bell upon a wrong password.

- i3lock uses PAM and therefore is compatible with LDAP etc.

Requirements
------------
- pkg-config
- libxcb
- libxcb-util
- libpam-dev
- libcairo-dev
- libxcb-composite0
- libxcb-composite0-dev
- libxcb-xinerama
- libev
- libx11-dev
- libx11-xcb-dev
- libxkbcommon >= 0.5.0
- libxkbcommon-x11 >= 0.5.0

##### Ubuntu

    sudo apt-get install pkg-config libxcb1 libpam-dev libcairo-dev libxcb-composite0 libxcb-composite0-dev libxcb-xinerama0-dev libev-dev libx11-dev libx11-xcb-dev libxkbcommon0 libxkbcommon-x11-0 libxcb-dpms0-dev libxcb-image0-dev libxcb-util0-dev libxcb-xkb-dev libxkbcommon-x11-dev libxkbcommon-dev
    
##### Aur Package
https://aur.archlinux.org/packages/i3lock-color-git

Running i3lock
--------------
Simply invoke the 'i3lock-color' command. To get out of it, enter your password and
press enter.

Example usage for colors:

i3lock-color --insidevercolor=0000a0bf --insidewrongcolor=ff8000bf --insidecolor=ffffffbf --ringvercolor=0020ffff --ringwrongcolor=4040ffff --ringcolor=404090ff --textcolor=ffffffff  --separatorcolor=aaaaaaff --keyhlcolor=30ccccff --bshlcolor=ff8000ff -r


Upstream
--------
Please submit pull requests for i3lock things to https://github.com/i3/i3lock and pull requests for color things to me at https://github.com/Arcaena/i3lock-color.
