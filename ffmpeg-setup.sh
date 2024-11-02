#!/bin/bash

# nvidia cuda options: --enable-cuda-nvcc --enable-cuvid --enable-ffnvcodec --enable-libnpp --enable-nvdec --enable-nvenc --enable-vdpau --enable-nonfree
#                      --nvccflags="-gencode arch=compute_86,code=sm_86 -O2"

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
	libdrm-dev glslang-dev libshaderc-dev libvpl-dev libplacebo-dev librav1e-dev librist-dev \
	libsvtav1dec-dev libsvtav1enc-dev \
	libiec61883-dev libraw1394-dev libavc1394-dev \
	yasm nasm

./configure --prefix=/opt/ffmpeg --enable-gpl --disable-stripping --disable-omx --enable-gnutls --enable-libaom --enable-libass --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libcodec2 --enable-libdav1d --enable-libflite --enable-libfontconfig --enable-libfreetype --enable-libfribidi --enable-libglslang --enable-libgme --enable-libgsm --enable-libharfbuzz --enable-libmp3lame --enable-libmysofa --enable-libopenjpeg --enable-libopenmpt --enable-libopus --enable-librubberband --enable-libshine --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx265 --enable-libxml2 --enable-libxvid --enable-libzimg --enable-openal --enable-opencl --enable-opengl --disable-sndio --enable-libvpl --disable-libmfx --enable-libdc1394 --enable-libdrm --enable-libiec61883 --enable-chromaprint --enable-frei0r --enable-ladspa --enable-libbluray --enable-libjack --enable-libpulse --enable-librabbitmq --enable-librist --enable-libsrt --enable-libssh --enable-libsvtav1 --enable-libx264 --enable-libzmq --enable-libzvbi --enable-lv2 --enable-sdl2 --enable-libplacebo --enable-librav1e --enable-pocketsphinx --enable-librsvg --enable-libjxl --enable-libv4l2 $@

make -j$(nproc)

sudo make install

