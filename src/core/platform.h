#ifndef PLATFORM_H
#define PLATFORM_H

// Platform abstraction layer — SDL2 + OpenGL via custom GL loader.

#include "gl_ext.h"

#ifdef __ANDROID__
    #include <android/log.h>
    #define LOG_TAG "VoxelEngine"
    #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
    #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
    #define LOGI(...) SDL_Log("[INFO]  " __VA_ARGS__)
    #define LOGE(...) SDL_Log("[ERROR] " __VA_ARGS__)
#endif

#endif // PLATFORM_H
