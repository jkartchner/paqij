#ifndef __paqij_h__ // if we already defined somewhere else, 
#define __paqij_h__ // don't do anything

#include <stdint.h>
#include <stdlib.h> // for malloc - TODO(jake): use mmap if you can?
//#include <math.h>   // TODO(jake): can you use this
#include "dbg.h"

#define local_persist static
#define internal static
#define global static
#define PCM_DEVICE "default"

#define Kilobytes(x) (x * 1024)
#define Megabytes(x) (Kilobytes(x) * 1024)
#define Gigabytes(x) (Megabytes((uint64_t)x) * 1024)

struct ThreadContext 
{
    int Placeholder;
};

typedef struct DebugFileOpenReadResult 
{
    void *file_contents;
    uint64_t file_size;

} DebugFileOpenReadResult;

typedef struct GameMemory 
{
    bool is_initialized;
    void *permanent_storage;          // NOTE: REQUIRED to be cleared to 0 at startup
    uint64_t permanent_storage_size;
    void *transient_storage;
    uint64_t transient_storage_size; // NOTE: REQUIRED to be cleared to 0 at startup
    DebugFileOpenReadResult *DEBUG_File_Read;

} GameMemory;

typedef struct GameScreenBuffer 
{
    int width;
    int height;
    int pitch;
    void *memory;

} GameBuffer;

typedef struct LoadedBitmap {
    uint32_t width;
    uint32_t height;
    uint32_t *pixels;

} LoadedBitmap;

typedef struct GameControllerInput { 
    int32_t mouse_buttons[5];
    int32_t mouse_x, mouse_y; // mouse position
    int button[10];
    // TODO(jake): check out casey's union and anon struct for this; why?
    /* 
     * button[0] = up direction
     * button[1] = down
     * [2] = left
     * [3] = right
     * [4] = enter
     * [5] = space
     * [6+] = reserved
     *
     */ 

    double target_seconds_per_frame;

}GameControllerInput;


void UpdateAndRender(ThreadContext *thread_context, 
                     GameMemory *game_memory, 
                     GameScreenBuffer *game_buffer, 
                     int frames_to_output, void *sound_buffer, 
                     GameControllerInput *keyboard_input, 
                     GameControllerInput *last_frame_keyboard_input);


DebugFileOpenReadResult DEBUGPlatformOpenReadEntireFile(
                     ThreadContext *thread_context, 
                     const char *file_name);

int DEBUGPlatformOpenWriteEntireFile(ThreadContext *thread_context, 
                     const char *file_name, 
                     uint64_t file_size, 
                     void *file_contents);

int DEBUGPlatformFreeMemory(ThreadContext *thread_context, void *memory);


#endif
