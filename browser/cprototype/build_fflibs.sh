cd ffmpeg

emconfigure ./configure --cc="emcc" --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
  --disable-programs --disable-inline-asm --disable-asm --disable-doc --disable-pthreads --disable-w32threads --disable-network \
  --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --enable-protocol=file \
  --disable-indevs --extra-cflags="-Wno-warn-absolute-paths"
emmake make

for l in libavformat libavdevice libavcodec libavutil; do
    cp $l/$l.so ../$l.so;
done
