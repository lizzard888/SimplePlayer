Simple audio player program with command-line interface. Works with .wav and .aac file formats.

Requriements:
- newest version of GStreamer:
https://gstreamer.freedesktop.org/
- gcc compiler:
https://gcc.gnu.org/

Installation:
Makefile included.

Usage:
- ./player -h     (show help)
- ./player <wav or aac file with extension> [-P optional - play immediately]

Examples:
- ./player song.wav -P
- ./player song.wav

Commands (via standard input):
- ( P ) - play / pause
- ( S ) - stop and quit
- ( + ) - volume up (10%)
- ( - ) - volume down (10%)
