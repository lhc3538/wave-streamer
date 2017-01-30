TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    wave_streamer.c \
    tools/alsa.c

HEADERS += \
    wave_streamer.h \
    tools/alsa.h
