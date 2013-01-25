#!/bin/sh

##--enable-frei0r --enable-libass 

OPTIONS="--enable-gpl --enable-version3 --disable-pthreads --enable-runtime-cpudetect --enable-avisynth --enable-bzlib --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libfreetype --enable-libgsm --enable-libmp3lame --enable-libnut --enable-libopenjpeg --enable-libopus --enable-librtmp --enable-libschroedinger --enable-libspeex --enable-libtheora --enable-libutvideo --enable-libvo-aacenc --enable-libvo-amrwbenc --enable-libvorbis --enable-libvpx --enable-libx264 --enable-libxavs --enable-libxvid --enable-zlib"

./configure ${OPTIONS}


