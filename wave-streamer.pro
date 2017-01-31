TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    wave_streamer.c \
    tools/alsa.c \
    tools/tcp.c \
    httpd.c

HEADERS += \
    wave_streamer.h \
    tools/alsa.h \
    tools/tcp.h \
    httpd.h \
    plugins/input.h \
    plugins/output.h
