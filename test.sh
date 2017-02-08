#!/bin/sh

export LD_LIBRARY_PATH="$(pwd)"

make clean

make

#./wave_streamer -i "./input_alsa.so" -o "./output_tcp.so"
./wave_streamer -i "./input_alsa.so" -o "./output_http.so"
