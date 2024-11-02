#!/bin/bash

set -e

sudo apt install -y build-essential libgnutls28-dev libmp3lame-dev libx264-dev libopus-dev libvpx-dev libssl-dev \
	libladspa-ocaml-dev libfrei0r-ocaml-dev \
	libaom-dev libass-dev libbluray-dev libbs2b-dev libcdio-dev libcodec2-dev \
	liblilv-dev libdc1394-dev \
	libchromaprint-dev \
	libcaca-dev \
	libdav1d-dev flite1-dev libgme-dev libv4l-dev libgsm1-dev \
	libmysofa-dev libopenjp2-7-dev libopenmpt-dev libpulse-dev \
	librabbitmq-dev librsvg2-dev librubberband-dev \
	libshine-dev libshine-ocaml-dev \
	libsnappy-dev libsoxr-dev libssh-dev libspeex-dev \
	libsrt-gnutls-dev \
	libtheora-dev libtwolame-dev libvidstab-dev libwebp-dev \
	libx265-dev libx264-dev \
	libxvidcore-dev libzimg-dev \
	libzmq3-dev libzvbi-dev liblv2dynparam1-dev libomxil-bellagio-dev libopenal-dev libopencl-clang-dev libopengl-dev libsdl2-dev libpocketsphinx-dev \
	opencl-dev \
	libjack-dev libcdio-paranoia-dev libcdio-dev \
	yasm nasm

./configure --prefix=/opt/ffmpeg --enable-gpl --disable-stripping --enable-gnutls --enable-ladspa --enable-libaom --enable-libass --enable-libbluray --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libcodec2 --enable-libdav1d --enable-libflite --enable-libfontconfig --enable-libfreetype --enable-libfribidi --enable-libgme --enable-libgsm --enable-libjack --enable-libmp3lame --enable-libmysofa --enable-libopenjpeg --enable-libopenmpt --enable-libopus --enable-libpulse --enable-librabbitmq --enable-librubberband --enable-libshine --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libsrt --enable-libssh --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx265 --enable-libxml2 --enable-libxvid --enable-libzimg --enable-libzmq --enable-libzvbi --enable-lv2 --enable-omx --enable-openal --enable-opencl --enable-opengl --enable-sdl2 --enable-pocketsphinx --enable-librsvg --enable-libdc1394 --enable-libdrm --enable-chromaprint --enable-frei0r --enable-libx264 --enable-libdrm --disable-rkmpp --enable-version3 --disable-libopenh264 --disable-vaapi --disable-vdpau --disable-decoder=h264_v4l2m2m --disable-decoder=vp8_v4l2m2m --disable-decoder=mpeg2_v4l2m2m --disable-decoder=mpeg4_v4l2m2m --enable-doc --enable-libv4l2 $@

make -j$(nproc)

sudo make install

