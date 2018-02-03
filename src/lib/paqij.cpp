#include "paqij.h"
#include "engine_math.h"



internal uint32_t RoundFloatToInt32(float Float);

#define M_PI 3.1415926535f
#define White       { 1.0f, 1.0f, 1.0f, 1.0f }
#define Black       { 0.0f, 0.0f, 0.0f, 1.0f }
#define Red         { 1.0f, 0.0f, 0.0f, 1.0f }
#define Green       { 0.0f, 1.0f, 0.0f, 1.0f }
#define Blue        { 0.0f, 0.0f, 1.0f, 1.0f }
#define Yellow      { 1.0f, 1.0f, 0.0f, 1.0f }
#define Orange      { 1.0f, 0.65f, 0.0f, 1.0f }
#define Purple      { 0.5f, 0.0f, 0.5f, 1.0f }


typedef struct GameState {
    v2 offset;
    int sound_buff_offset;
    int tone_hz;
    LoadedBitmap loaded_bitmap;
    LoadedBitmap loaded_pigeon_standing;
}GameState;

struct Question {
    int difficulty;
    char *question_text;
    char *answer_text[];  // standard to have first string in array be correct answer

};

#pragma pack( push, 1 )
typedef struct BMPHeader {
    uint16_t type;        // chars "BM"
    uint32_t file_size;        // size of file in bytes
    uint16_t reserved1;   // unused
    uint16_t reserved2;   // unused
    uint32_t data_offset;   // offset to start of pixel 
    uint32_t header_size; 	//	Header Size - Must be at least 40
    uint32_t width; 	    //	Image width in pixels
    uint32_t height;	    //	Image height in pixels
    uint16_t planes; 	    //	Must be 1
    uint16_t bit_count; 	//	Bits per pixel - 1, 4, 8, 16, 24, or 32
    uint32_t compression;   //	Compression type (0 = uncompressed)
    uint32_t image_size; 	//	Image Size - may be zero for uncompressed images
    uint32_t preferred_X_px_per_meter;//	Preferred resolution in pixels per meter
    uint32_t preferred_Y_px_per_meter;//	Preferred resolution in pixels per meter
    uint32_t color_maps_used;  	//	Number Color Map entries that are actually used
    uint32_t colors;     	//	Number of significant colors
} BMPHeader;
#pragma pack(pop)

typedef struct Color {
    float red;
    float green;
    float blue;
    float alpha;
} Color;


internal uint32_t
GetColorInt(Color color)
{
    unsigned char red = RoundFloatToInt32(color.red * 255);
    unsigned char green = RoundFloatToInt32(color.green * 255);
    unsigned char blue = RoundFloatToInt32(color.blue * 255);
    unsigned char alpha = RoundFloatToInt32(color.alpha * 255);
    return alpha << 24 | red << 16 | green << 8 | blue;
}

internal uint32_t
RoundFloatToInt32(float Float)
{
    int32_t result = (int32_t)(Float + 0.5f);
    return result;
}

internal void
DrawRectangle(GameScreenBuffer *screen_buffer, v2 offset, v2 max, Color color)
{
    int32_t min_x = RoundFloatToInt32(offset.x);
    int32_t max_x = RoundFloatToInt32(max.x + offset.x);
    int32_t min_y = RoundFloatToInt32(offset.y);
    int32_t max_y = RoundFloatToInt32(max.y + offset.y);

    if(min_x < 0) {
        min_x = 0;
    }
    if(min_y < 0) {
        min_y = 0;
    }

    if(max_x > screen_buffer->width) { // not writing past buffer by writing width
        max_x = screen_buffer->width;
    }

    if(max_y > screen_buffer->height) {
        max_y = screen_buffer->height;
    }

    uint8_t *Row = (uint8_t *)screen_buffer->memory + min_x * 4 + 
                                min_y * screen_buffer->pitch; // start at top left of rect
    for(int y = min_y; y < max_y; ++y) {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int x = min_x; x < max_x; ++x) { 
            *Pixel++ = GetColorInt(color);//0x000000FF;
        }
        Row += screen_buffer->pitch;
    }
}

/// Casey Muratori passes the debug function callback as a pointer to the game
/// This callback is loaded in the game memory struct or game state struct
/// I don't see the point of doing that over just making a function call into the platform
/// TODO(jake): make this implementation more robust and able to handle shifted byte 
/// orders. See day 38 in handmade hero for good example of this.
internal LoadedBitmap
LoadBMP(ThreadContext *thread_context, char *file_name)
{
    DebugFileOpenReadResult read_result = DEBUGPlatformOpenReadEntireFile(thread_context, 
                                                    file_name);
    LoadedBitmap result = {};
    if(read_result.file_size > 0)
    {
        BMPHeader *bmp_header = (BMPHeader *)read_result.file_contents; // can cold cast
        // IMPORTANT: rows must end on 4 byte boundary; no problem here since pixel = 4 b
        uint32_t *pixels = (uint32_t *)
            ((uint8_t *)read_result.file_contents+bmp_header->data_offset); 
        // above: we first cast to byte pointer so we can add the header in bytes to that
        result.pixels = pixels;
        result.width = bmp_header->width;
        result.height = bmp_header->height;
        check_debug(bmp_header->compression == 0, 
                "Compressed bitmap: %d; Probably can't handle that!", 
                bmp_header->compression);

        // NOTE(jake): If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EITHER DIRECTION and the
        // height will be negative for top-down.
        // (Also, there can be compression, etc., etc... DON'T think this is 
        // a complete BMP loading code because it isn't!
        
        for(unsigned int y = 0; y < bmp_header->width; ++y) {
            for(unsigned int x = 0; x < bmp_header->height; ++x) { 
                *pixels = (*pixels >> 8) | (*pixels << 24);
                ++pixels;
            }
        }
    }
    return result;
}
    
/// Skipping sound in this tone comes from the poor algorithm here to generate
/// the wave. The closer to 0 during the generation of the wave, the more
/// unstable the tone. I reset to 0 every frame....
internal int
GameGenerateSineWave(int frames_to_output, void *sound_buffer, int tone_hz)
{
    int l1; 
    //float amplitude = 0.0113 * UINT16_MAX; // amplitude determines volume
    uint16_t *temp_buff = (uint16_t *)sound_buffer;
    for(l1 = 0; l1 < frames_to_output; l1 += 2) { 
       /* if(tone_hz > 100) {
                            
            uint16_t temp = (uint16_t)(amplitude * 
                                    sin((2 * M_PI * (l1)
                                    * tone_hz) / 44100));
            temp_buff[l1] = temp;       // need to do left and right channel for stereo
            temp_buff[l1 + 1] = temp;   // need to do left and right channel for stereo
        } else {
         */   //temp_buff[l1] = (uint16_t) amplitude; // turning off sound 
            temp_buff[l1] = random() & 0xff;
            temp_buff[l1 + 1] = random() & 0xff;
        //} 
    }
    
    return 0;
}


internal int
GameGenerateWhiteNoise(int frames_to_output, void *sound_buffer)
{
    int l1; 
    uint16_t *temp_buff = (uint16_t *)sound_buffer;
    for(l1 = 0; l1 < frames_to_output; l1 += 2) { 
        temp_buff[l1] = random() & 0xff;
        temp_buff[l1 + 1] = random() & 0xff;
    }
    
    return 0;
}

internal int
GameGenerateSilence(int frames_to_output, void *sound_buffer)
{
    int l1; 
    uint16_t *temp_buff = (uint16_t *)sound_buffer;
    for(l1 = 0; l1 < frames_to_output; l1 += 2) { 
        temp_buff[l1] = 0x0000;
        temp_buff[l1 + 1] = 0x0000;
    }
    
    return 0;
}
internal int
GameOutputSound(ThreadContext *thread_context, 
                int frames_to_output, 
                void *sound_buffer, 
                int tone_hz)
{
    (void)thread_context;
    GameGenerateWhiteNoise(frames_to_output, sound_buffer);
    return 0;
}


// TODO(jake): fix this by checking how casey does it
internal void 
ProcessPlayerInput(GameControllerInput *keyboard_input, GameControllerInput *last_frame_keyboard_input, GameState *game_state)
{
   if(keyboard_input->button[0]){ // up button
      game_state->offset.y -= 10; 
   }

   if(keyboard_input->button[1]){ // down button
      game_state->offset.y += 10; 
   }

   if(keyboard_input->button[2]){ // left button
      game_state->offset.x -= 10; 
   }

   if(keyboard_input->button[3]){ // right button
      game_state->offset.x += 10; 
   }

   if(keyboard_input->button[4] && !last_frame_keyboard_input->button[4]){ // enter button
      game_state->tone_hz = 200;
   }

   if(keyboard_input->button[5] && !last_frame_keyboard_input->button[5]){ // space button
      game_state->tone_hz = 0;
   }
   if(keyboard_input->mouse_buttons[0] && !last_frame_keyboard_input->mouse_buttons[0]) {
       game_state->tone_hz = 261;
   }

}

internal void
DrawBitmap(GameScreenBuffer *screen_buffer, 
        LoadedBitmap *bitmap, 
        v2 offset)
{

    int32_t x_offset = RoundFloatToInt32(offset.x);
    int32_t y_offset = RoundFloatToInt32(offset.y);
    int32_t x_source_offset = 0;
    int32_t y_source_offset = 0;

    int32_t blit_width = RoundFloatToInt32((float)bitmap->width + x_offset);
    int32_t blit_height = RoundFloatToInt32((float)bitmap->height + y_offset);

    if(blit_height < 0 || blit_width < 0) // do nothing if nothing on screen to draw
        return;

    if(x_offset < 0) 
    {
        x_source_offset = x_offset;
        x_offset = 0;
    }

    if(y_offset < 0) 
    {
        y_source_offset = y_offset;
        y_offset = 0;
    }

    if(blit_width > screen_buffer->width)
    {
        blit_width = screen_buffer->width;
    } 

    if (blit_height > screen_buffer->height)
    {
        blit_height = screen_buffer->height;
    }

    // NOTE: the y and x source offset values are necessary if you want a bitmap to draw
    // partially off the screen when offsets are below 0; otherwise, this is just 
    // accouting for the bitmap format being stored upside down
    uint32_t *source_row = bitmap->pixels + bitmap->width * 
                        (bitmap->height - 1 + y_source_offset) - x_source_offset;
    uint8_t *dest_row = (uint8_t *)screen_buffer->memory + 
                        (x_offset * 4 + y_offset * screen_buffer->pitch);

    for(int y = y_offset; y < blit_height; ++y) 
    {
        uint32_t *dest = (uint32_t *)dest_row;
        uint32_t *source = source_row;
        for(int x = x_offset; x < blit_width; ++x) 
        { 
            float salpha = ((*source >> 24) & 0xFF) / 255.0f;
            float sred = ((*source >> 16) & 0xFF);
            float sgreen = ((*source >> 8) & 0xFF);
            float sblue = ((*source >> 0) & 0xFF);
            
            float dred = ((*dest >> 16) & 0xFF);
            float dgreen = ((*dest >> 8) & 0xFF);
            float dblue = ((*dest >> 0) & 0xFF);

            // TODO(casey): need to talk about premultiplied alpha; 
            // this is not premultiplied alpha
            float res_red = (1.0f-salpha)*dred + salpha * sred;
            float res_green = (1.0f-salpha)*dgreen + salpha * sgreen;
            float res_blue = (1.0f-salpha)*dblue + salpha * sblue;
            /*if((*source >> 24) > 20)
            {
                *dest = *source;
            }*/
            *dest = (((uint32_t)(res_red + 0.5f) << 16) |
                     ((uint32_t)(res_green + 0.5f) << 8 ) |
                     ((uint32_t)(res_blue + 0.5f) << 0));
            ++dest;
            ++source;
        }

        dest_row += screen_buffer->pitch;
        source_row -= bitmap->width;
    }
}


void UpdateAndRender(ThreadContext *thread_context,
                    GameMemory *game_memory,
                    GameScreenBuffer *screen_buffer, 
                    int frames_to_output, 
                    void *sound_buffer, 
                    GameControllerInput *keyboard_input, 
                    GameControllerInput *last_frame_keyboard_input)
{
    (void)thread_context;
    GameState *game_state = (GameState *)game_memory->permanent_storage; 

    if(!game_memory->is_initialized)
    {
        char file_to_read2[] = "/home/jake/Projects/paqij/data/pigeon-standing.bmp";
        // TODO(jake): get rid of mem leak here; need to dispose of bitmap file in mem
        game_state->loaded_pigeon_standing = LoadBMP(thread_context, file_to_read2);
        game_state->tone_hz = 50;
        game_state->offset.x = 0;
        game_state->offset.y = 0;
        game_memory->is_initialized = 1;
    }

    ProcessPlayerInput(keyboard_input, last_frame_keyboard_input, game_state);

    Color mouse_pointer_color = Orange;
    Color clear_screen_color = White; // RGBA

    DrawRectangle(screen_buffer, V2(0.0f, 0.0f), 
                 V2((float)screen_buffer->width, 
                 (float)screen_buffer->height), 
                 clear_screen_color);

    DrawRectangle(screen_buffer, 
                  V2((float)keyboard_input->mouse_x - 5.0f, 
                  (float)keyboard_input->mouse_y - 5.0f), 
                  V2(10.f, 10.0f), mouse_pointer_color); 

    DrawBitmap(screen_buffer, 
               &game_state->loaded_pigeon_standing, 
               game_state->offset);

    GameOutputSound(thread_context, frames_to_output, sound_buffer, game_state->tone_hz);

}
