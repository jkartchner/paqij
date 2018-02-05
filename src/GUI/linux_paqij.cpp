/* ============================================================================
   $File: $linux_paqij.cpp
   $Date: $02-01-2018
   $Revision: $
   $Creator: Jacob Kartchner $
   $Notice: (c) Copyright 2017 by Jacob Kartchner. All Rights Reserved. $
   =========================================================================== */

/*
 * TODO(jake): THIS IS NOT A FINAL PLATFORM LAYER
 *
 * saved game locations
 * getting a handle to our own executable file
 * asset loading path
 * threating (launch a thread)
 * ClipCursor () (for multimonitor support)
 * what to do when the app is no longer active
 * Blit speed improvements
 * Hardware acceleration (OpenGL)
 *
 * This is just a partial list of stuff
 *
 * (may expand in future: sound on separate thread, etc.)
 *
 */
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>
#include <time.h>       // TODO(jake): look into do_gettimeofday(&time) in kernel space
#include <sys/stat.h>   // open() and file metadata -- TODO(jake): use linux/fs.h? filp
#include <fcntl.h>      // flags for open()            TODO(jake): use linux/fs.h?
#include <unistd.h>     // for file read and close()   TODO(jake): use linux/fs.h?
#include "../lib/paqij.h"


#define XA_ATOM ((Atom) 4)
  

/*
 * Information used by the visual utility routines to find desired visual
 * type from the many visuals a display may support.
 */

typedef struct {
    Visual *visual;
    VisualID visualid;
    int screen;
    int depth;
#if defined(__cplusplus) || defined(c_plusplus)
    int c_class;                                  /* C++ */
#else
    int class;
#endif
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;


typedef struct XGameWindow {
    Display *display;     // a set of screens for a single user with one
                          // keyboard and one pointer (usually a mouse)
    Window x_window;
    int x_screen;         // a physical monitor and hardware that can be color,
                          // grayscale, or monochrome. There can be multiple
                          // screens for each display
    XVisualInfo x_visualinfo;
    int width = 1920;
    int height = 1080;
    int color_depth = 24; // 3 bytes for color and 8 bits hard coded for padding
    void *bitmap_memory;
    XImage *x_image;
    Atom wmDeleteMessage;
    XFontStruct *font;
} XGameWindow;


struct MwmHints 
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
};

enum 
{
    MWM_HINTS_FUNCTIONS = (1L << 0),
    MWM_HINTS_DECORATIONS =  (1L << 1),

    MWM_FUNC_ALL = (1L << 0),
    MWM_FUNC_RESIZE = (1L << 1),
    MWM_FUNC_MOVE = (1L << 2),
    MWM_FUNC_MINIMIZE = (1L << 3),
    MWM_FUNC_MAXIMIZE = (1L << 4),
    MWM_FUNC_CLOSE = (1L << 5)
};


typedef struct ALSAObject {
	int error;
	snd_pcm_t *handle;
	uint8_t *buff;
    uint8_t *buff_ptr;
    int buff_position = 0;

	unsigned int rate 	 = 44100;
	int channels             = 2;
    int expected_frames_to_output;
    int bytes_per_sample = 2;
} ALSAObject;


global XGameWindow x_game_window;
global ALSAObject alsa_object;
global struct timespec g_time_since_last_frame;
global int g_FPS;
global float g_sound_latency;






internal void
SetupandMapXWindow()
{
    x_game_window.display = XOpenDisplay(NULL);
    Assert(x_game_window.display);   // X returns null if unsuccessful

    // is a macro in X for XDefaultScreen(display) { Display *display; }
    x_game_window.x_screen = DefaultScreen(x_game_window.display);
    x_game_window.x_window = XCreateSimpleWindow(x_game_window.display, 
            RootWindow(x_game_window.display, x_game_window.x_screen), 
            10, 10, x_game_window.width, x_game_window.height, 1,
            BlackPixel(x_game_window.display, x_game_window.x_screen), 
            WhitePixel(x_game_window.display, x_game_window.x_screen));

    Assert(XMatchVisualInfo(
                x_game_window.display, x_game_window.x_screen, 
                32, TrueColor, &x_game_window.x_visualinfo));


    // set the mask of the type of X msgs we want our window to receive
    XSelectInput(
            x_game_window.display, 
            x_game_window.x_window, 
            ExposureMask | KeyPressMask | KeyReleaseMask | ResizeRedirectMask |
            ButtonPressMask | ButtonReleaseMask | PointerMotionMask); 

#if FULLSCREEN // pass OPTFLAGS=-DFULLSCREEN to force fullscreen
    // IMPORTANT: this is code to make a full screen window at launch. This is
    // not resolution independent, though, so I'm not sure how useful it will be
    // if resolution is changed
    Atom atoms[2] = { XInternAtom(x_game_window.display, 
            "_NET_WM_STATE_FULLSCREEN", False), None };

    int test = XChangeProperty( x_game_window.display,
                     x_game_window.x_window,
                     XInternAtom(x_game_window.display,
                                 "_NET_WM_STATE", False),
                     XA_ATOM, 32, PropModeReplace, 
                     (unsigned char *)atoms, 1);
#else
    // NOTE(jake): these are HINTS for the window manager, not X11. I haven't found
    // a WM yet which would not resize, even if the MWM_FUNC_RESIZE isn't specified
    // Apparently applications which can force a fixed size are using GTK or above
    // so they can manipulate the WM and X a little more tightly. (?) Regardless,
    // it's clear that Xlib does not provide a way to prevent resizing without my
    // writing some intercept code and forcing a resize every time they move the screen.
    // This ultimately degrades performance per the fps drop bug when window resized


    struct MwmHints hints;
    Atom wm = XInternAtom(x_game_window.display, "_MOTIF_WM_HINTS", False);
    hints.functions = MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE | 
                      MWM_FUNC_CLOSE    | MWM_FUNC_MOVE;
    hints.flags = MWM_HINTS_FUNCTIONS;
    check_debug(XChangeProperty(x_game_window.display, 
                x_game_window.x_window, wm, XA_ATOM,
                32, PropModeReplace, (unsigned char *)&hints, 5) 
                == 1, "Failed");
#endif

    XStoreName(x_game_window.display, x_game_window.x_window, "Handmade");
    XMapWindow(x_game_window.display, x_game_window.x_window);
    
    // next two lines required to safely close window with X in top right corner
    x_game_window.wmDeleteMessage = XInternAtom(x_game_window.display,  
                                                "WM_DELETE_WINDOW", False);

    XSetWMProtocols(x_game_window.display, x_game_window.x_window, 
                    &x_game_window.wmDeleteMessage, 1);

    // now load the font
    const char *fontname = "6x13bold";
    x_game_window.font = XLoadQueryFont(x_game_window.display, fontname);

    if(!x_game_window.font) 
    {
        x_game_window.font = XLoadQueryFont(x_game_window.display, "fixed");
        debug("Font load failed. Reverting to fixed.");
    }

    XSetFont(x_game_window.display, 
            DefaultGC(x_game_window.display, x_game_window.x_screen),
            x_game_window.font->fid);

    XAutoRepeatOff(x_game_window.display);

}

internal void
SizeXWindowBackBuffer()
{
    // TODO(Jake): consider using mmap instead of malloc - faster?
    x_game_window.bitmap_memory = malloc(x_game_window.width * x_game_window.height * 4); 
    check_mem(x_game_window.bitmap_memory);
    x_game_window.x_image = XCreateImage(
            x_game_window.display, 
            x_game_window.x_visualinfo.visual, 
            x_game_window.color_depth, 
            ZPixmap, 
            0, (char *)x_game_window.bitmap_memory, 
            x_game_window.width, x_game_window.height,
            8, 0);
}

internal int
InitializePlaySound()
{
    // NOTE(jake): sound spikes at begin of playback means latency set too high
    // or sample output isn't high enough
	alsa_object.buff = (uint8_t *) calloc(1, 40000); // as small as poss to prev latency
    check_mem(alsa_object.buff);
    alsa_object.buff_ptr = alsa_object.buff;

    // TODO(Jake): make more robust alsa init with near checks and 
    // device initialization checks; this is the simplest way but also most
    // error prone if there's something off in the sound drivers/hw
    // See ~/Projects/ALSA../bin/sound_...c for more info
    alsa_object.error = snd_pcm_open(&alsa_object.handle, 
            PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK); 


    check_debug(alsa_object.error == 0, "Playback open error: %s\n", 
            snd_strerror(alsa_object.error));


    alsa_object.error = snd_pcm_set_params(alsa_object.handle,
                                           SND_PCM_FORMAT_S16_LE,         // pcm format
                                           SND_PCM_ACCESS_RW_INTERLEAVED, // PCM access
                                           alsa_object.channels,          // PCM channels
                                           alsa_object.rate,              // PCM rate
                                           1,                          // allow resampling
                                           89000);     // latency -
    // success returns 0
    check_debug(alsa_object.error == 0, "Playback open error: %s\n", 
                        snd_strerror(alsa_object.error))     // time 2getsamples2buff

    return 0;
}

/// Bottom Line: non-blocking mode, not asynch or sys io writes (which lock kernel)
internal int
PlaySoundFrames(int frames_to_output) 
{
    if(frames_to_output <= 0)
    {
        return 0;
    }

    int frames;
    alsa_object.buff_ptr = alsa_object.buff;
    
    frames = snd_pcm_writei(alsa_object.handle, 
                            alsa_object.buff_ptr, 
                            frames_to_output);
    if(frames == -11)
        debug("buffer overrun!");
    if(frames == -32)
    {   // simplest recovery 
        frames = snd_pcm_recover(alsa_object.handle, frames, 0); 
        check_debug(frames > 0, "sound write failed: %d: %s", 
                frames, snd_strerror(frames));
    }
    return frames;
}

internal int
PollSoundCard(void)
{
    snd_pcm_sframes_t avail_frames;
    snd_pcm_sframes_t delay_frames;

    check(!snd_pcm_avail_delay(alsa_object.handle, &avail_frames, &delay_frames), 
            "Failed to poll the sound card.");

    g_sound_latency = (float)delay_frames / (float)alsa_object.rate;
    if(avail_frames > 0)
    {
        return (int)avail_frames;
    }
    else
    {
        return -1;
    }

  error:
    return alsa_object.expected_frames_to_output;
}

// call this function to start a nanosecond resolution timer
internal struct timespec timer_start() {
    struct timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time); // CLOCK_PROCESS_CPUTIME_ID
    return start_time;
}

// call this funciton to end a timer, returning nanoseconds elapsed
internal long timer_end(struct timespec start_time) {
    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    if(end_time.tv_nsec < start_time.tv_nsec) { // protect against var wrapping
        return ((1000000000 - start_time.tv_nsec) + end_time.tv_nsec);
    } else {
        return (end_time.tv_nsec - start_time.tv_nsec);
    }
}

internal void timer_sleep(long nanos_to_wait)
{
    struct timespec sleep_time = {};
    struct timespec recover_sleep_time = {};
    int test = 0;
    sleep_time.tv_nsec = nanos_to_wait; 
    test = nanosleep(&sleep_time, &recover_sleep_time);
    while(test == -1){ // trickier than it looks; can't use same struct for both
        test = nanosleep(&recover_sleep_time, &sleep_time);
    } 
}

// TODO(jake): pass in the memory arena
// and load the file into that
DebugFileOpenReadResult DEBUGPlatformOpenReadEntireFile(ThreadContext *thread_context,
                                                        const char *file_name)
{
    (void)thread_context;
    struct stat st;
    stat(file_name, &st);
    void *file_contents = malloc(st.st_size);
    check_mem(file_contents);

    DebugFileOpenReadResult result = {};

    int file_handle = open(file_name, O_RDONLY);
    check(file_handle, "File open bombed.");
    check(read(file_handle, file_contents, (int)st.st_size) == (int)st.st_size, 
          "Didn't read the file correctly.");
    check_debug(!close(file_handle), "Didn't close the file correctly."); // 0 == success

    result.file_contents = file_contents;
    result.file_size = (uint64_t)st.st_size;
    return result;

  error:
    if(file_contents)
        free(file_contents);
    return result;
}

int DEBUGPlatformOpenWriteEntireFile(ThreadContext *thread_context,
                                     const char *file_name, 
                                     uint64_t file_size, 
                                     void *file_contents)
{
    (void)thread_context;
    int file_handle = open(file_name, O_WRONLY|O_CREAT, S_IRWXU);
    check(file_handle, "File open for write bombed.");
    check((uint64_t)write(file_handle, file_contents, file_size) == file_size, 
            "Didn't write the size of the file.");
    check_debug(!close(file_handle), "Didn't close the file correctly on write.");
    return file_size;

  error:
    return -1;
}

int DEBUGPlatformFreeMemory(ThreadContext *thread_context, void *memory)
{
    (void)thread_context;
    check(memory, "Failed to free memory from non-platform specific code.");
    free(memory);
    return 0;

  error:
    return -1;
}

internal GameMemory
InitializeGameMemory(GameMemory game_memory)
{
    // TODO(jake): 
    //
    //  Use mmap to allocate memory here. malloc calls mmap eventually anyway
    //  Use NDEBUG ifndef to try to put mapped memory in fixed location
    //  Use O_DIRECT flag with your open() and file IO;
    //  This reads file to virt memory & no memcpy to ram; it's 4x faster
    //  You can get filesize with posix stndrd with stat function
    //  Because O_DIRECT may impose alignment restrictions, you'll need
    //  to look into posix_memalign to get memory aligned to proper blk bounds
    //  See page in evernote on O_DIRECT for use of O_DIRECT example
    //
    //  You could use mmap to map the file directly to memory, which is most
    //  efficient for the DEBUGPlatform IO functions. But that won't work
    //  for reading a file directly into virt or other memory you already have.
    //  Will stick with strategy of all mem being mapped up front for long term
    //
    game_memory.permanent_storage_size = Megabytes(64); 
    game_memory.permanent_storage = calloc(1, game_memory.permanent_storage_size);
    check_mem(game_memory.permanent_storage);

    game_memory.transient_storage_size = Gigabytes(1);
    game_memory.transient_storage = calloc(1, game_memory.transient_storage_size);    
    check_mem(game_memory.transient_storage);
    return game_memory;
}

int main(void) 
{
    SetupandMapXWindow();
    XEvent e;
    SizeXWindowBackBuffer();

    InitializePlaySound();

    
    /*char file_to_read[] = "/home/jake/Projects/paqij/linux_handmade.cpp";
    char file_to_write[] = "/home/jake/Projects/paqij/write_test.out";
    DebugFileOpenReadResult file_handle = DEBUGPlatformOpenReadEntireFile(file_to_read);
    DEBUGPlatformOpenWriteEntireFile(file_to_write, 
                                     file_handle.file_size, file_handle.file_contents); 
    if(file_handle.file_contents)
        free(file_handle.file_contents);
    */ 
    GameMemory game_memory =  {};
    game_memory = InitializeGameMemory(game_memory);

    GameControllerInput keyboard_input = {};
    GameControllerInput last_frame_keyboard_input = {};

    // TODO(Jake):use xrandr exntension api ? to get monitor refresh rate to enforce fps
    int monitor_refresh_hz = 60;
    int game_update_hz = monitor_refresh_hz / 2;

    double target_seconds_elapsed_per_frame = 
                                           (1000.0f / (double)game_update_hz) / 1000.0f;

    keyboard_input.target_seconds_per_frame = target_seconds_elapsed_per_frame; 
    g_time_since_last_frame = timer_start(); // start perf timing

    alsa_object.expected_frames_to_output = (alsa_object.rate / game_update_hz) * 2;

    ThreadContext thread_context = {}; // this will be used for multi-threading later
    int frames_to_output = 0;
    while(1) {
        XNextEvent(x_game_window.display, &e);


        if (e.type == Expose) {
            GameScreenBuffer game_buffer = {};
            game_buffer.memory = x_game_window.bitmap_memory;
            game_buffer.height = x_game_window.height;
            game_buffer.width = x_game_window.width;
            game_buffer.pitch = x_game_window.width * 4;
            frames_to_output = PollSoundCard(); 
               
            // artifacts w/o extra frames
            // When playing audio, will need to somehow also pass along any 
            // frames that were NOT sent to the sound card (the write return value)
            // Consider pulling sound generation out of the game loop and calling
            // directly by the platform layer so we don't have to preserve values
            // through the game loop and the poll. We can poll and deal with the 
            // result immediately
            frames_to_output = frames_to_output > 0 ? frames_to_output + 800 : 0;
            UpdateAndRender(&thread_context, &game_memory, &game_buffer, 
                    frames_to_output, alsa_object.buff, 
                    &keyboard_input, &last_frame_keyboard_input);
            last_frame_keyboard_input = keyboard_input;

            PlaySoundFrames(frames_to_output); 

            // timer goes here to hold page flip to end of frame/begin of next
            double elapsed_time = timer_end(g_time_since_last_frame) / 1000000000; 
            if(elapsed_time < target_seconds_elapsed_per_frame) {
                while(elapsed_time < target_seconds_elapsed_per_frame) {
                    timer_sleep((long)(target_seconds_elapsed_per_frame * 
                            1000000000) - timer_end(g_time_since_last_frame)); 
                    elapsed_time = (double)timer_end(g_time_since_last_frame) / 1000000000;
                }
            } else {
                // TODO(jake): Missed frame rate!!
                debug("Frame rate missed!");
                exit(1);
                // TODO(jake): lower rate (not below 30) if miss too many frames
                // TODO(jake): logging
            }
            g_FPS = 1 / elapsed_time;
            g_time_since_last_frame = timer_start();

            // TODO: see if you can change the width/height of the target destination
            // so that X stretches the underlying image for you. I doubt it
            XPutImage(
                    x_game_window.display, 
                    x_game_window.x_window, 
                    DefaultGC(
                        x_game_window.display, 
                        x_game_window.x_screen), 
                    x_game_window.x_image, 0, 0, 0, 0, 
                    x_game_window.width, 
                    x_game_window.height);


            char fps_string[20];
#if NDEBUG
            sprintf(fps_string, "FPS: %d\n", g_FPS);
            XDrawImageString(x_game_window.display,  // XDrawString doesn't use bg white
                      x_game_window.x_window,
                      DefaultGC(
                          x_game_window.display,
                          x_game_window.x_screen),
                      10, 20, 
                      fps_string, 7);
#endif

           
            // TODO(Jake): look if XSendEvent is faster
            XClearArea(x_game_window.display, x_game_window.x_window, 
                    0, 0, 1, 1, True);
        }
        
        // TODO(jake): Fix this by checking how casey does it -- all the way from the begin
        if (e.type == KeyPress) {
            if(e.xkey.keycode == 9) {
                break; // exit
            }
            switch(e.xkey.keycode) {
                case 111: 
                    {
                        keyboard_input.button[0] = 1; // up
                        break;
                    }
                case 116: 
                    {
                        keyboard_input.button[1] = 1; // down
                        break;
                    }
                case 113: 
                    {
                        keyboard_input.button[2] = 1; // left
                        break;
                    }
                case 114: 
                    {
                        keyboard_input.button[3] = 1; // right
                        break;
                    }
                case 36: 
                    {
                        keyboard_input.button[4] = 1; // enter
                        break;
                    }
                case 65: 
                    {
                        keyboard_input.button[5] = 1; // space
                        break;
                    }
                case 95:
                    { // <F11> key pressed
#if FULLSCREEN
#else
                        // IMPORTANT: this changes to fullscreen, but MAINTAINS the 
                        // monitor settings they were running in; if you want to change 
                        // resolution you could try to find something like 
                        // ChangeDisplaySettings in windows. I prefer to use neither and 
                        // stretch pixels up rather than force a change of the graphics
                        // mode of the monitor - no flicker, hdmi sync issues, etc.
                        // Note that intel integrated graphics chips usually will be 
                        // better off if you actually change the dis resolution, though

                        
                        XEvent e_fullscreen;
                        e_fullscreen.xclient.type = ClientMessage;
                        e_fullscreen.xclient.window = x_game_window.x_window;
                        e_fullscreen.xclient.message_type = XInternAtom(
                                x_game_window.display, "_NET_WM_STATE", True);
                        e_fullscreen.xclient.format = 32;
                        e_fullscreen.xclient.data.l[0] = 2;
                        e_fullscreen.xclient.data.l[1] = XInternAtom(
                                x_game_window.display, "_NET_WM_STATE_FULLSCREEN", True);
                        e_fullscreen.xclient.data.l[2] = 0;
                        e_fullscreen.xclient.data.l[3] = 1;
                        e_fullscreen.xclient.data.l[4] = 0;

                        XSendEvent(x_game_window.display, 
                                    DefaultRootWindow(x_game_window.display), 
                                    False, 
                                    SubstructureRedirectMask | 
                                    SubstructureNotifyMask, 
                                    &e_fullscreen);
#endif
                        break;
                    }
                default: 
                    {
                        break;
                    }

            }
        }
        
        if(e.type == KeyRelease) {
            switch(e.xkey.keycode) {
                case 111: 
                    {
                        keyboard_input.button[0] = 0; // up
                        break;
                    }
                case 116: 
                    {
                        keyboard_input.button[1] = 0; // down
                        break;
                    }
                case 113: 
                    {
                        keyboard_input.button[2] = 0; // left
                        break;
                    }
                case 114: 
                    {
                        keyboard_input.button[3] = 0; // right
                        break;
                    }
                case 36: 
                    {
                        keyboard_input.button[4] = 0; // enter
                        break;
                    }
                case 65: 
                    {
                        keyboard_input.button[5] = 0; // space
                        break;
                    }
                default: 
                    {
                        break;
                    }

            }
        }

        if(e.type == MotionNotify) {
            keyboard_input.mouse_x = e.xmotion.x;
            keyboard_input.mouse_y = e.xmotion.y;
        }
        
        if(e.type == ButtonPress) {
            if(e.xbutton.button == Button1) {
                keyboard_input.mouse_buttons[0] = 1;
            } else if(e.xbutton.button == Button3) { // Button2 is the mouse wheel
                keyboard_input.mouse_buttons[2] = 1;
            }
        }

        if(e.type == ButtonRelease) {
            if(e.xbutton.button == Button1) {
                keyboard_input.mouse_buttons[0] = 0;
            } else if(e.xbutton.button == Button3) {
                keyboard_input.mouse_buttons[2] = 0;
            }
        }

        if(e.type == ResizeRequest) //was ConfigureNotify;lag bug gone bc overridden here
        {

        }

        if(e.type == ClientMessage) {
            if((Atom)e.xclient.data.l[0] == x_game_window.wmDeleteMessage)
                break;
        }

    }

    XAutoRepeatOn(x_game_window.display);
    snd_pcm_close(alsa_object.handle);
    free(alsa_object.buff);
    free(x_game_window.bitmap_memory);
    free(game_memory.permanent_storage);
    free(game_memory.transient_storage);
    XFree(x_game_window.x_image);
    XCloseDisplay(x_game_window.display);
    return 0;

}

