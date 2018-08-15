
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
#CONFIG -= qt

SOURCES += \
    yuv_rgb.cpp \
    ffmpeg_save_image.cpp \
    ffmpeg_1.cpp \
    sdl.cpp \
    sdl2.cpp
#DESTDIR+= ../


UI_DIR+= ./tmp
RCC_DIR+= ./tmp
MOC_DIR+= ./tmp
OBJECTS_DIR+= ./tmp


INCLUDEPATH += $$PWD/../ffmpeg/include \
               $$PWD/../SDL2/include



DEPENDPATH += $$PWD/../ffmpeg/include


LIBS += -L$$PWD/../ffmpeg/lib/ -lavcodec\
        -L$$PWD/../ffmpeg/lib/ -lavdevice\
        -L$$PWD/../ffmpeg/lib/ -lavfilter\
        -L$$PWD/../ffmpeg/lib/ -lavformat\
        -L$$PWD/../ffmpeg/lib/ -lavutil\
        -L$$PWD/../ffmpeg/lib/ -lswscale\
        -L$$PWD/../ffmpeg/lib/ -lswresample\
        -L$$PWD/../ffmpeg/lib/ -lpostproc\
        -L$$PWD/../SDL2/lib/libSDL2.lib

HEADERS += \
    main.h




