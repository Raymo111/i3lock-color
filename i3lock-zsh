#compdef i3lock

_i3lock() {
  integer ret=1
  local -a args
  zstyle ':completion:*:*:i3lock:*:descriptions' format ''
  args+=(
    "(--version -v)"{--version,-v}"[Display the version of i3lock]"
    "(--nofork -n)"{--nofork,-n}"[Don't fork after starting]"
    "(--beep -b)"{--beeping,-b}"[Enable beeping]"
    "(--no-unlock-indicator -u)"{--no-unlock-indicator,-u}"[Disable the unlock indicator]"
    "(--image -i)"{--image,-i}"[Display the given PNG image instead of a blank screen]:filename:_files -g '*.(png|jpg)'"
    "--raw[Read the image given by --image as a raw image instead of PNG]:raw:"
    "(--color -c)"{--color,-c}"[Turn the screen into the given hex color]:hex:->hex"
    "(--tiling -t)"{--tiling,-t}"[Image will be displayed tiled all over the screen]"
    "(--centered -C)"{--centered,-C}"[Image will be displayed centered on the screen]"
    "(--fill -F)"{--fill,-F}"[Image will fill all over the screen]"
    "(--max -M)"{--max,-M}"[Image will fit the screen at width or height]"
    "(--scale -L)"{--scale,-L}"[Image will be stretched on the screen]"
    "(--pointer -p)"{--pointer,-p}"[Sets mouse pointer type]:pointer:(win default)"
    "(--ignore-empty-password -e)"{--ignore-empty-password,-e}"[Do not validate empty password]"
    "(--show-failed-attempts -f)"{--show-failed-attempts,-f}"[Show the number of failed attemps]"
    "--debug[Enables debug logging.]"

    # i3lock-color OPTIONS

    "(--screen -S)"{--screen,-S}"[Specifies which display to draw the unlock indicator]:int:"
    "(--blur -B)"{--blur,-B}"[Captures the screen and blurs it using the given sigma]:sigma:"
    "(--clock --force-clock -k)"{--clock,--force-clock,-k}"[Displays the clock]"
    "--indicator[Forces the indicator to always be visible]"
    "--radius[The radius of the circle]:float:"
    "--ring-width[The width of the ring unlock indicator]:float:"
    # Colors
    "--inside-color[Sets the default \"resting\" color for the interior circle]:hex:->hex"
    "--ring-color[Sets the default ring color]:hex:->hex"
    "--insidever-color[Sets the interior circle color while the password is being verified]:hex:->hex"
    "--ringver-color[Sets the ring color while the password is being verified]:hex:->hex"
    "--insidewrong-color[Sets the interior circle color for suring flash for an incorrect password]:hex:->hex"
    "--ringwrong-color[Sets the ring color during the flas for an incorrect password]:hex:->hex"
    "--line-color[Sets the color for the line separating the inside circle, and the outer ring]:hex:->hex"
    "--line-uses-inside[Conflicts with --line-uses-ring. Overrides --linecolor; the line will match the inside color]"
    "--line-uses-ring[Conflicts with --line-uses-inside. Overrides --linecolor; The line will match the ring color]"
    "--keyhl-color[Sets the color of the ring 'highlight' strokes that appear upon keypress]:hex:->hex"
    "--bshl-color[Sets the color of the ring 'highlight' strokes that appear upon backspace]:hex:->hex"
    "--separator-color[Sets the color of the 'separator', which is at both ends of the ring highlights]:hex:->hex"
    "--verif-color[Sets the color of the status text while verifying]:hex:->hex"
    "--wrong-color[Sets the color of the status when \"wrong\"]:hex:->hex"
    "--modif-color[Sets the color of the status text while verifying]:hex:->hex"
    "--layout-color[Sets the color of the keyboard layout text]:hex:->hex"
    "--time-color[Sets te color of the time in the clock]:hex:->hex"
    "--date-color[Sets the color of the date in the clock]:hex:->hex"
    "--greeter-color[Sets the color of the greeter text]:hex:->hex"
    # Text
    "--time-str[Sets the format used for generating the time string]:str:"
    "--date-str[Sets the format used for generating the date string]:str:"
    "--verif-text[Sets the string to be shown while verifying]:str:"
    "--wrong-text[Sets the string to be shown upon entering an incorrect password]:str:"
    "--keylayout[Displays the keylayout]:mode:((0\:'Displays the full string returned by the query, i.e. English (US)' 1\:'Displays up until the first parenthesis, i.e. English' 2\:'Displays just the contents of the parenthesis, i.e US'))"
    "--noinput-text[Sets the string to be shown upon pressing backspace whithout anything to delete]:str:"
    "--lock-text[Sets the string to be shown while acquiring pointer and keyboard focus]:str:"
    "--lockfailed-text[Sets the string to be shown after failing to acquire pointer and keyboard focus]:str:"
    "--greeter-text[Sets the greeter text]:str:"
    "--no-modkey-text[Hides the modkey indicator]"
    # Align
    "(--time-align --date-align --layout-align --verif-align --wrong-align --modif-align --greeter-align)"{--time-align,--date-align,--layout-align,--verif-align,--wrong-align,--modif-align,--greeter-align}"[Sets the text alignment]:alignment:((0\:'default' 1\:'left aligned' 2\:'right aligned'))"
    # Outline color
    "(--timeoutline-color --dateoutline-color --layoutoutline-color --verifoutline-color --wrongoutline-color --modifoutline-color --greeteroutline-color)"{--timeoutline-color,--dateoutline-color,--layoutoutline-color,--verifoutline-color,--wrongoutline-color,--modifoutline-color,--greeteroutline-color}"[Sets the color of the outline]:hex:->hex"
    # Fonts
    "(--time-font --date-font --layout-font --verif-font --wrong-font --greeter-font)"{--time-font,--date-font,--layout-font,--verif-font,--wrong-font,--greeter-font}"[Sets the font used to render various strings]:str:"
    # Size
    "(--time-size --date-size --layout-size --verif-size --wrong-size --greeter-size)"{--time-size,--date-size,--layout-size,--verif-size,--wrong-size,--greeter-size}"[Sets the font size used to render various strings]:int:"
    # Outline width
    "(--timeoutline-width --dateoutline-width --layoutoutline-width --verifoutline-width --wrongoutline-width --modifieroutline-width --greeteroutline-width)"{--timeoutline-width,--dateoutline-width,--layoutoutline-width,--verifoutline-width,--wrongoutline-width,--modifieroutline-width,--greeteroutline-width}"[Sets the width of the outline]:float:"
    # Position
    "--ind-pos[Sets the position for the unlock indicator]:pos:->ind_pos"
    "--time-pos[Sets the position for the time string]:pos:->time_pos"
    "--date-pos[Sets the position for the date string]:pos:->date_pos"
    "--greeter-pos[Sets the position for the greeter string]:pos:->greeter_pos"
    # Media keys
    "--pass-media-keys[Allow media keys to be used while the screen is locked]"
    "--pass-screen-keys[Allow screen keys to be used while the screen is locked]"
    "--pass-power-keys[Allow power keys to be used while the screen is locked]"
    "--pass-volume-keys[Allow volume keys to be used while the screen is locked]"
    "--custom-key-commands[Enable shell commands for media keys]"
    "--cmd-brightness-up[Command for XF86MonBrightnessUp]"
    "--cmd-brightness-down[Command for XF86MonBrightnessDown]"
    "--cmd-media-play[Command for XF86AudioPlay]"
    "--cmd-media-pause[Command for XF86AudioPause]"
    "--cmd-media-stop[Command for XF86AudioStop]"
    "--cmd-media-next[Command for XF86AudioNext]"
    "--cmd-media-prev[Command for XF86AudioPrev]"
    "--cmd-audio-mute[Command for XF86AudioMute]"
    "--cmd-volume-up[Command for XF86AudioRaiseVolume]"
    "--cmd-volume-down[Command for XF86AudioLowerVolume]"
    "--cmd-mic-mute[Command for XF86AudioMicMute]"
    "--cmd-power-down[Command for XF86PowerDown]"
    "--cmd-power-off[Command for XF86PowerOff]"
    "--cmd-power-sleep[Command for XF86Sleep]"
    # Bar mode
    "--bar-indicator[Replaces the usual ring indicator with a bar indicator]"
    "--bar-direction[Sets the direction the bars grow in]:direction:((0\:'default' 1\:'reverse' 2\:'both'))"
    "--bar-orientation[Sets whether the bar is vertically or horizontally oriented]:orientation:(vertical horizontal)"
    "--bar-step[Sets the step that each bar decreases by when a key is pressed]:int:"
    "--bar-max-height[The maximum height a bar can get to]:float:"
    "--bar-base-width[The thickness of the \"base\" bar that all the bar originate from]:float:"
    "--bar-color[Sets the default color of the bar base]:hex:->hex"
    "--bar-periodic-step[The value by which the bars decrease each time the screen is redrawn]:int:"
    "--bar-pos[Sets the bar position]:pos:->bar_pos"
    "--bar-count[Sets the number of minibars to draw on each screen]:int:"
    "--bar-total-width[The total width of the bar]:float:"
    # Extra configs
    "--redraw-thread[Starts a separate thread for redrawing the screen]"
    "--refresh-rate[The refresh rate of the indicator]:double:"
    "--composite"
    "--no-verify[Do not verify the password provided by the user and unlock inmediately]"
    # Slideshow
    "--slideshow-interval[The interval to wait until switching to the nex image]:double:"
    "--slideshow-random-selection[Randomize the order of the images]"


  )
  _arguments $args[@] && ret=0

  case "$state" in
    hex)
      zstyle ':completion:*:*:i3lock:*:descriptions' format '%d'
      _message "Color in hexadecimal rrggbbaa, like #ff0000ff or #354F9AFF"
      ;;
    ind_pos)
      zstyle ':completion:*:*:i3lock:*:normal' format '%d'
      zstyle ':completion:*:*:i3lock:*:descriptions' format '%B%d%b'

      _message "\"x position:y position\""
      _message -e "normal" "'x' - x position of the current display. Corresponds to the left-most row of pixels"
      _message -e "normal" "'y' - y position of the current display. Corresponds to the topmost row of pixels"
      _message -e "normal" "'w' - width of the current display"
      _message -e "normal" "'w' - height of the current display"
      _message -e "normal" "'r' - unlock indicator radius"
      ;;
    time_pos)
      zstyle ':completion:*:*:i3lock:*:normal' format '%d'
      zstyle ':completion:*:*:i3lock:*:descriptions' format '%B%d%b'

      _message "\"x position:y position\""
      _message -e "normal" "All the variables from --ind-pos may be used, in addition to:"
      _message -e "normal" "'ix' - x position of the indicator on the current display"
      _message -e "normal" "'iy' - y position of the indicator on the current display"
      _message -e "normal" "If the --bar-indicator option is used, the following may be used"
      _message -e "normal" "'bw' - width od the bar indicator"
      _message -e "normal" "'bx' - x position of the bar indicator on the current display"
      _message -e "normal" "'by' - y position of the bar indicator on the current display"
      ;;
    date_pos)
      zstyle ':completion:*:*:i3lock:*:normal' format '%d'
      zstyle ':completion:*:*:i3lock:*:descriptions' format '%B%d%b'

      _message "\"x position:y position\""
      _message -e "normal" "All the variables from --ind-pos and --time-pos may be used, in addition to:"
      _message -e "normal" "'tx' - x position of the timestring on the current display"
      _message -e "normal" "'ty' - y position of the timestring on the current display"
      ;;
    greeter_pos)
      zstyle ':completion:*:*:i3lock:*:normal' format '%d'
      zstyle ':completion:*:*:i3lock:*:descriptions' format '%B%d%b'

      _message "\"x position:y position\""
      _message -e "normal" "All the variables from --ind-pos and --time-pos may be used"
      ;;
    bar_pos)
      zstyle ':completion:*:*:i3lock:*:normal' format '%d'
      zstyle ':completion:*:*:i3lock:*:descriptions' format '%B%d%b'

      _message "\"x position:y position\""
      _message -e "normal" "All the variables from --ind-pos and --time-pos may be used"
      _message -e "normal" "If only one number is provided, sets the vertical offset from the top or left edge"
      _message -e "normal" "If two numbers are provided, sets the starting position of the bar"
      ;;


  esac

  return ret
}

_i3lock
