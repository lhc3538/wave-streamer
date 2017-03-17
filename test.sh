#!/bin/sh

export LD_LIBRARY_PATH="$(pwd)"

make clean

make

./wave_streamer -i "./input_alsa.so" -o "./output_udp.so"
#./wave_streamer -i "./input_alsa.so" -o "./output_http.so"
#./wave_streamer -i "./input_alsa.so" -o "./output_file.so"
