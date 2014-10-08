First attempt to build a video player with sdl/ffmpeg.

Build with `make hybrid` and run with `./hybrid $some_video`. Uses libavdevice
(which uses SDL) to output the video and handles audio playback directly with
SDL.
