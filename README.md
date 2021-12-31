Remap keystrokes using /dev/input and /dev/uinput.

Works on Android with adb or root. Public domain code (but linked against the copyrighted
Bionic library for Android use). Probably works on other Linux systems.

updown [--skip-location location] [--no-skip] [-v] REMAP LIST...
 where REMAP LIST... is a list of entries of the form:
  x y                : remap key code x (decimal) to y; if y is -1, disable key x
  x cmd SHELLCOMMAND : run SHELLCOMMAND when key code x is pressed
  
For keycodes, see https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

When run with no arguments, it remaps VOLUME UP/DOWN to PAGE UP/DOWN, and by default it skips
any input device whose location is ALSA. On my phone this lets me remap the hardware buttons.

For instance:

  updown 114 cmd "am broadcast -a net.tasker.VOLUMEDOWNPRESSED"
  
broadcasts a net.tasker.VOLUMEDOWNPRESSED intent (which you can catch in Tasker) whenever the VOLUME DOWN 
key is pressed.

This is a quick and dirty program, and there are various limitations. For instance,
if /dev/input has a device provides both the relevant keystrokes and some other functionality like
mouse or touch, the extra functionality of that device will be lost.