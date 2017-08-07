i3lock - improved screen locker
===============================

_This is just a re-patched version of i3lock with the commits from [i3lock-color](https://github.com/eBrnd/i3lock-color); all the credit for the color functionality goes to [eBrnd](https://github.com/eBrnd/) !_

![i3lock-color in action. Why are you reading this?](https://github.com/chrjguill/i3lock-color/raw/master/screenshot.png "Screenshot sample")

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
	- The following additional options have been added:
     - `-S, --screen` -- specifies which display to draw the unlock indicator on
     - `-k, --clock` -- enables the clock display.
		 - `--indicator` -- forces the indicator to always show, even if there's no activity.
		 - `--composite` -- enables checking for compositors and trying to grab the compositor window, since that causes issues with some compositors.
		    - **NOTE**: This can potentially allow sensitive information to display over the screen locker, so take care when you use this option.
     - `-B=sigma, --blur` -- enables Gaussian blur. Sigma is the blur radius.
	      - Note: You can still composite images over the blur (but still under the indicator) with -i.
				- Eventually there might be an `imagepos` arg, similar to `time` and `datepos`. 
     - `--indpos="x+(w/2):y+(h/2)"` -- position of the unlock indicator. Expressions using the variables x (current screen's x value), y (current screen's y value), w (screen width), h (screen height), and r (indicator radius) can be used.
     - `--timestr="%H:%M:%S"` -- allows custom overriding of the time format string. Accepts `strftime` formatting. Default is `"%H:%M:%S"`.
     - `--timepos="ix:iy-20"` -- position of the time. All the variables in `indpos` can be used here, as well as the additional values ix (indicator x position), iy (indicator y position), cw (clock width), and ch (clock height).
     - `--timecolor=rrggbbaa` -- color of the time string
     - `--timefont="sans-serif"` -- font used for the time display
     - `--timesize=32` -- font size for the time display
     - `--datestr="%A, %m %Y"` -- allows custom overriding of the date format string. Accepts `strftime` formatting. Default is `"%A, %m %Y"`.
     - `--datepos="ix:iy-20"` -- position of the date. All the variables in `indpos` and `timepos` can be used here, as well as the additional values tx (time x) and ty (time y).
     - `--datecolor=rrggbbaa` -- color of the date string
     - `--datefont="sans-serif"` -- font used for the date display
     - `--datesize=14` -- font size for the date display
		 - `--veriftext="verifying…"` -- text to be shown while verifying
		 - `--wrongtext="wrong!"` -- text to be shown upon an incorrect password being entered
		 - `--textsize=28` -- font size for the status text
		 - `--modsize=14` -- font size for the modifier keys listing
		 - `--radius=90` -- the radius of the circle indicator

- You can specify whether i3lock should bell upon a wrong password.

- i3lock uses PAM and therefore is compatible with LDAP etc.
  On OpenBSD i3lock uses the bsd\_auth(3) framework.
## Installation 
In order to install this re-patched version you'll need to have the following requirements: 

### Requirements
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

##### Debian/Ubuntu 

    sudo apt-get install pkg-config libxcb1 libpam-dev libcairo-dev libxcb-composite0 libxcb-composite0-dev libxcb-xinerama0-dev libev-dev libx11-dev libx11-xcb-dev libxkbcommon0 libxkbcommon-x11-0 libxcb-dpms0-dev libxcb-image0-dev libxcb-util0-dev libxcb-xkb-dev libxkbcommon-x11-dev libxkbcommon-dev
 
 then open a terminal and clone the github repo like this :
 ```sh 
 user@machine:/tmp$ git clone https://github.com/chrjguill/i3lock-color
 user@machine:/tmp$ cd i3lock-color/
 user@machine:/i3lock-color$make  
 user@machine:/i3lock-color$sudo make install 
 ```
 Now test the i3lock-color options
##### Aur Package
https://aur.archlinux.org/packages/i3lock-color-git

Running i3lock
-------------
Simply invoke the 'i3lock' command. To get out of it, enter your password and
press enter.

A [sample script](https://github.com/chrjguill/i3lock-color/blob/master/lock.sh) is included in this repository. [Here](https://streamable.com/fpl46) is a short clip of that script in action!

On OpenBSD the `i3lock` binary needs to be setgid `auth` to call the
authentication helpers, e.g. `/usr/libexec/auth/login_passwd`.

Upstream
--------
Please submit pull requests for i3lock things to https://github.com/i3/i3lock and pull requests for color things to me at https://github.com/chrjguill/i3lock-color.
