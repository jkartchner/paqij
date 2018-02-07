#include "paqij.h"
#include "engine_math.h"

/***************************************************************
 * Ideas:
 *      S02E02: zoom into eye, kinai tancol lock, almodik arrol
 *      reszt vesz a csoportban - legyen ugy a galamb a 
 *      keksszel.
 *      S02??: Western - eagle cry, using other for cover
 */

internal uint32_t RoundFloatToInt32(float Float);

#define M_PI 3.1415926535f
#define White        1.0f, 1.0f, 1.0f, 1.0f 
#define Black        0.0f, 0.0f, 0.0f, 1.0f 
#define Red          1.0f, 0.0f, 0.0f, 1.0f 
#define Green        0.0f, 1.0f, 0.0f, 1.0f 
#define Blue         0.0f, 0.0f, 1.0f, 1.0f 
#define Yellow       1.0f, 1.0f, 0.0f, 1.0f 
#define Orange       1.0f, 0.65f, 0.0f, 1.0f 
#define Purple       0.5f, 0.0f, 0.5f, 1.0f 

typedef struct GameState {
    v2 offset;
    int sound_buff_offset;
    int tone_hz;
    MemoryArena memory_arena;
    LoadedBitmap loaded_pigeon_standing;
    LoadedWave loaded_test_wave;
}GameState;

struct Question {
    int difficulty;
    char *question_text;
    char *answer_text[];  // standard to have first string in array be correct answer

};

#pragma pack( push, 1 )
typedef struct BMPHeader {
    uint16_t type;          // chars "BM"
    uint32_t file_size;     // size of file in bytes
    uint16_t reserved1;     // unused
    uint16_t reserved2;     // unused
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

typedef struct WAVEHeader {
    uint32_t chars_RIFF;   // R I F F
    uint32_t adjusted_file_size; // this is the file size - 8
    uint32_t chars_WAVE;   // W A V E
    // Following is subchunk1 of the format; usually a sep struct in robust implem
    uint32_t chars_fmt_;   // f m t space
    uint32_t data_length;  // this is usually 16
    uint16_t data_type;    // 1 is PCM
    uint16_t channels;     // total channels
    uint32_t sample_rate;  // 44100 per second
    uint32_t bytes_per_second; // (sample rate * bits per channel * channels) / 8 (176400)
    uint16_t frame_rate;   // (bits per sample * channels) / 8 - spec callsit"block align"
    uint16_t bits_per_sample; // 16
    // Following is subchunk2  of format; usually a sep struct in robust implementations
    uint32_t chars_DATA;   // D A T A
    uint32_t data_size;    // total size of the pcm data in bytes
    // the PCM data starts after the data_size
} WAVEHeader;
#pragma pack(pop)


internal LoadedWave
LoadWAVE(ThreadContext *thread_context, MemoryArena *memory_arena, char *file_name)
{
    (void)thread_context;
    DebugFileOpenReadResult read_result = DEBUGPlatformOpenReadEntireFile(thread_context, 
                           (uint8_t *)memory_arena->used, 
                           (uint32_t)((memory_arena->base + memory_arena->size)
                               - memory_arena->used), 
                           file_name);
    LoadedWave result = {};
    if(read_result.file_size > 0)
    {
        memory_arena->used = (uint32_t *)(memory_arena->used + read_result.file_size);
        WAVEHeader *wave_header = (WAVEHeader *)read_result.file_contents;
        uint16_t *pcm_data = (uint16_t *)((uint8_t *)&wave_header->data_size + 4);
        // above: we first cast to byte pointer so we can add the header in bytes to that
        result.pcm_data = pcm_data;
        result.pcm_ptr = pcm_data;
        result.data_size = wave_header->data_size;
        
        debug("Sample rate %d", wave_header->sample_rate);
        // NOTE(jake): If you are using this generically for some reason,
        // there can be compression, etc., etc... DON'T think this is 
        // a complete WAVE loading code because it isn't!
    }
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
LoadBMP(ThreadContext *thread_context, MemoryArena *memory_arena, char *file_name)
{
    DebugFileOpenReadResult read_result = DEBUGPlatformOpenReadEntireFile(thread_context, 
                           (uint8_t *)memory_arena->used, 
                           (uint32_t)((memory_arena->base + memory_arena->size)
                               - memory_arena->used), 
                           file_name);
    LoadedBitmap result = {};
    if(read_result.file_size > 0)
    {
        memory_arena->used = (uint32_t *)(memory_arena->used + read_result.file_size);
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
                int sound_output_last_frame,
                void *sound_buffer, 
                LoadedWave *loaded_wave)
{
    (void)thread_context;
    uint16_t *sound_data_end = (uint16_t *)((uint8_t *)loaded_wave->pcm_data + 
                                                    loaded_wave->data_size);
    uint16_t *data_projected_end = (uint16_t *)
                                   ((uint8_t *)loaded_wave->pcm_ptr + 
                                    (sound_output_last_frame * 4));

    int adjusted_frames_to_output = 0;
    bool will_overrun = false;

    if(sound_data_end > data_projected_end)
    {
        // sound_output_last_frame * 2 b/c we have two channels of bytes to advance
        loaded_wave->pcm_ptr = data_projected_end;
    }
    else 
    {
        loaded_wave->pcm_ptr = loaded_wave->pcm_data + 
            (data_projected_end - sound_data_end);
    }

    // this definition must come here, after the pcm_ptr has been moved
    uint16_t *output_projected_end = (uint16_t *)
                                   ((uint8_t *)loaded_wave->pcm_ptr + 
                                   (frames_to_output * 4));
    if(sound_data_end > output_projected_end)
    {
        adjusted_frames_to_output = frames_to_output;
    }
    else
    {
        adjusted_frames_to_output = (sound_data_end - loaded_wave->pcm_ptr);
        will_overrun = true;
    }

    uint16_t *temp_buff = (uint16_t *)sound_buffer;
    for(int l1 = 0; l1 < adjusted_frames_to_output; l1 += 2) 
    { 
        temp_buff[l1] = loaded_wave->pcm_ptr[l1];           // left channel
        temp_buff[l1 + 1] = loaded_wave->pcm_ptr[l1 + 1];   // right channel
    }
    if(will_overrun)
    {
        int adjusted2 = frames_to_output - adjusted_frames_to_output;
        for(int l1 = 0; l1 < adjusted2; l1 += 2) 
        { 
            temp_buff[l1 + adjusted_frames_to_output] = loaded_wave->pcm_data[l1]; 
            temp_buff[l1 + 1 + adjusted_frames_to_output] = loaded_wave->pcm_data[l1 + 1];
        }
    }
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
                    int frames_sent_last_frame, 
                    GameControllerInput *keyboard_input, 
                    GameControllerInput *last_frame_keyboard_input)
{
    (void)thread_context;
    GameState *game_state = (GameState *)game_memory->permanent_storage; 

    if(!game_memory->is_initialized)
    {
        char file_to_read2[] = "/home/jake/Projects/paqij/data/pigeon-standing.bmp";
        char file_to_read3[] = "/home/jake/Projects/paqij/data/test.wav";
        game_state->tone_hz = 50;
        game_state->offset = { 0, 0 }; // topleft of the screen
        game_state->memory_arena.base = (uint32_t *)game_state + sizeof(game_state), 
        game_state->memory_arena.used = (uint32_t *)game_state + sizeof(game_state),
        game_state->memory_arena.size = game_memory->permanent_storage_size - 
                                                         sizeof(game_state); 
        game_state->loaded_pigeon_standing = LoadBMP(thread_context, 
                &game_state->memory_arena, file_to_read2);
        
        game_state->loaded_test_wave = LoadWAVE(thread_context, 
                &game_state->memory_arena, file_to_read3);
        game_memory->is_initialized = 1;
    }

    ProcessPlayerInput(keyboard_input, last_frame_keyboard_input, game_state);

    DrawRectangle(screen_buffer, V2(0.0f, 0.0f), 
                 V2((float)screen_buffer->width, 
                 (float)screen_buffer->height), 
                 GetColor(White));

    DrawRectangle(screen_buffer, 
                  V2((float)keyboard_input->mouse_x - 5.0f, 
                  (float)keyboard_input->mouse_y - 5.0f), 
                  V2(10.f, 10.0f), GetColor(Blue)); 

    DrawBitmap(screen_buffer, 
               &game_state->loaded_pigeon_standing, 
               game_state->offset);

    GameOutputSound(thread_context, 
            frames_to_output, 
            frames_sent_last_frame,
            sound_buffer, 
            &game_state->loaded_test_wave);

}
