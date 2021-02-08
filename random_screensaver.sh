#!/bin/bash
# grab a random picture from unsplash and process for betterlockscreen

mkdir -p /var/tmp/backgrounds
XSAVER=`pgrep xscreensaver`

while true ; do

  while pgrep i3lock 
  do
    if [ `pgrep i3lock | wc -l` -lt 2 ]; then
      killall i3lock
    else

      echo "screen locked"
      DIMENSIONS=`xdpyinfo | grep dimensions | awk '{print $2}'`
      wget -q -O /tmp/wallpaper.jpg https://source.unsplash.com/$DIMENSIONS/?travel,nature \
        && mv /tmp/wallpaper.jpg /var/tmp/backgrounds/
     
      #sometimes it tries to process the file too quickly, so we wait 
      sleep 1
      echo "wallpaper downloaded. refreshing i3lock cache"
      betterlockscreen -u /var/tmp/backgrounds/wallpaper.jpg
      
      sleep 1
      echo "reloading i3lock"
      I3LOCK=`pgrep i3lock | head -n 1` && echo $I3LOCK && kill -USR1 $I3LOCK
      
      #wait 30 minutes till next refresh
      sleep 1800
    fi
  done
  sleep 5 #slow down how often it checks for i3lock

done

