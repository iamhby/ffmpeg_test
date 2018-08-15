
#ifndef __MAIN_H__
#define __MAIN_H__


#include "stdio.h"
#include "stdlib.h"
#include "string.h"
using namespace std;

typedef unsigned char _uint8;

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/pixfmt.h>
#include <libavutil/log.h>

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_types.h>
#include <SDL_name.h>
#include <SDL_main.h>
#include <SDL_config.h>
#include <SDL.h>
#include <SDL_thread.h>
}

#define HBY_RUN 3

#undef main

#endif
