A simple PowerShell script that allows to quickly change audio playback device and refresh rate.

## Features
* Toggle between a set of audio playback devices by name.
* Set an audio playback device by name.
* Toggle between a set of refresh rates.
* Set a refresh rate.

## Dependencies
* PowerShell cmdlets in https://github.com/Sycobob/DisplaySettings-PowerShell-Cmdlets must be installed.
* PowerShell cmdlets in https://github.com/Sycobob/WindowsAudioDevice-Powershell-Cmdlet must be installed.

## Notes
This is a very crude script that I threw together in a day to make gaming slightly more convenient. I knew nothing of PowerShell going in, so there is a huge amount of room for improvement. However, it serves it's purpose so further development will be sporadic at best.

## TODO
* Fix the issue where icons in the system tray do not always disappear.
* Fix the issue where system tray bubbles are queued instead of replacing/adding to existing bubbles (need some way to maintain state between script invokations).
* Improve the way the audio playback device is set by name. Currently it requires an exact name, but it would be better to find the closest match. This will help with the fact that names sometimes changes when a device is plugged into a different USB port.
* Make the script faster. Currently the keybind > VBScript > PowerShell script flow is pretty slow.

## Attribution
Audio device cmdlets forked from <a href="https://github.com/cdhunt/WindowsAudioDevice-Powershell-Cmdlet">cdhunt</a>.
Refresh rate cmdlets modified from <a href="http://ajdotnet.wordpress.com/2008/01/19/command-line-tool-vs-powershell-cmdlet/">ajdotnet</a>.
Icon pulled from <a href="http://www.wpclipart.com/signs_symbol/icons_oversized/Info_icon.png.html">wpclipart</a>