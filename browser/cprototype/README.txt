First attempt to build a video player with sdl/ffmpeg.

Assuming you have sdl and ffmpeg installed, build with `make` and use 
with `./play $some_video`. Frames are not really synchronised yet and it 
lacks audio support. But if you're feeling adventurous you can build with 
`make audio` and listen the ambient noise generator.

I also discovered that ffmpeg's libavdevice already supports sdl as an output
device and will look into that before I update this one.
