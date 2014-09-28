First attempt to build a video player with sdl/ffmpeg.

Assuming you have sdl and ffmpeg installed, build with `make` and use
with `./play $some_video`. Frames are not really synchronised yet and it
lacks audio support. But if you're feeling adventurous you can build with
`make audio` and listen to the ambient noise generator.

With `make avdev` you can build a version that uses libavdevice. It won't work
though because seemingly the SDL output format in ffmpeg does not support
audio. Getting just the video to work is not difficult though.
