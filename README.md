Remap keystrokes using /dev/input and /dev/uinput.

Works on Android with adb or root. Public domain code (but linked against the copyrighted
Bionic library for Android use). Probably works on other Linux systems.

To install on Android:

  adb push updown /data/local/tmp
  adb shell chmod 755 /data/local/tmp/updown
  
To run on Android, use /data/local/tmp/updown whenever these instructions say "updown".

Instructions:

updown [-h] [--(only|reject|add)-(location|bus|name) xxx ...] [-v] REMAP LIST...
 where REMAP LIST... is a list of entries of the form:
  x y                : remap key code x (decimal) to y; if y is -1, disable x
  x cmd SHELLCOMMAND : run SHELLCOMMAND when key code x is pressed
If no arguments given, rejects location ALSA and remaps VOLUME UP/DOWN to PAGE UP/DOWN.
  
For keycodes, see https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

Devices are only remapped if they match the filters. By default all devices match.

Filtering can be done by attributes of location (location), bus number (bus) or device name (name). By
default all input devices

 --add-xxx yyy:    Accept any device whose xxx attribute matches yyy, regardless of what any other filters say.
 --only-xxx yyy:   Reject devices whose xxx attribute does not match yyy (unless --add overrode).
 --reject-xxx yyy: Reject devices whose xxx attribute matches yyy (unless --add overrode)
 
When run with no arguments, it remaps VOLUME UP/DOWN to PAGE UP/DOWN, and by default it skips
any input device whose location is ALSA. On my phone this lets me remap the hardware buttons. THis
is equivalent to: 

  updown --reject-location ALSA 114 109 115 104 

For another example:

  updown 114 cmd "am broadcast -a net.tasker.VOLUMEDOWNPRESSED"
  
broadcasts a net.tasker.VOLUMEDOWNPRESSED intent (which you can catch in Tasker) whenever the VOLUME DOWN 
key is pressed.

This is a quick and dirty program, and there are various limitations. For instance,
if /dev/input has a device provides both the relevant keystrokes and some other functionality like
mouse or touch, the extra functionality of that device will be lost.