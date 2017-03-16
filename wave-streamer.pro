TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    wave_streamer.c \
    tools/alsa.c \
    tools/tcp.c \
    plugins/input_alsa/input_alsa.c \
    plugins/output_http/output_http.c \
    plugins/output_tcp/output_tcp.c \
    utils.c \
    plugins/output_http/httpd.c \
    plugins/output_file/output_file.c \
    plugins/output_websocket/output_websocket.c \
    plugins/input_dsp/input_dsp.c \
    plugins/input_oss/input_oss.c

HEADERS += \
    wave_streamer.h \
    tools/alsa.h \
    tools/tcp.h \
    plugins/input.h \
    plugins/output.h \
    plugins/output_tcp/output_tcp.h \
    utils.h \
    plugins/output_http/httpd.h \
    plugins/output_file/output_file.h \
    plugins/output_websocket/output_websocket.h

DISTFILES += \
    wave_streamer \
    plugins/input_alsa/input_alsa.so \
    plugins/output_tcp/output_tcp.so \
    input_alsa.so \
    output_tcp.so \
    start.sh \
    test.sh \
    plugins/output_file/output_tcp.so \
    plugins/output_http/output_http.so \
    output_http.so \
    test.wav \
    plugins/output_file/output_file.so \
    output_file.so \
    www/audio.html \
    www/index.html \
    www/note.xml \
    www/note.html \
    www/audio.js \
    www/test.js
