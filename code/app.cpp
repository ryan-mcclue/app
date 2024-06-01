// SPDX-License-Identifier: zlib-acknowledgement
#if !TEST_BUILD
#define PROFILER 1
#endif

#include "base/base-inc.h"
#include <raylib.h>

#include "app.h"


// TODO:
      // menu to select:
      //   - rainbow
      //   - circles
      //   - audio source, e.g. miniaudio microphone, OS mixer etc.  
      //   - comments for vis. suggestions: https://www.youtube.com/watch?v=1pqIg-Ug7bU&list=PLpM-Dvs8t0Vak1rrE2NJn8XYEJ5M7-BqT&index=7&t=669s 
      
      
// 1. convert samples from time to frequency domain
// f32 in; (index is time)
// f32 complex out; (so index is frequency; amplitude and phase of a frequency component)
// (as both amp and phase, keep track of sin and cos; concisely with eulers complex formula)

// display logarithmically as ear heres
// this will result in having to downsample frequency ranges (just take max?)

// (lower frequencies are more impactful)

// if you have a 'real' signal with differing frequencies, e.g.
// f = 3.12f, there will be jumps/tears in waveform when frequency changes
// dft assumes signal is repeatable ad infinitum.
// so, it will extrapolate phantom frequencies at tear points 
// multiplying by hann window, i.e. 1Hz flipped cosine, smooths these tears
// the cost is adding 1Hz signal, whose effect is neglible
// the resultant frequency graph will be more tight, i.e. less spread out




// 1. draw visualisation
// 2. segment screen with region visualisation

// 1. scope variables
// 2. merge into state struct with arenas and .h file


INTERNAL f32
hann_function(f32 sample, f32 t)
{
  f32 h = 0.5f - 0.5f * F32_COS(F32_TAU * t);
  return sample * h;
}

// NOTE(Ryan): Modified from https://rosettacode.org/wiki/Fast_Fourier_transform#Python
// cooley-tukey algorithm O(nlogn) for pow2
INTERNAL void 
fft(f32 *in, u32 stride, f32z *out, u32 n)
{
  if (n == 1) 
  {
    out[0] = in[0];
    return;
  }

  fft(in, stride*2, out, n/2);
  fft(in + stride, stride*2, out + n/2, n/2);

  for (u32 k = 0; k < n/2; ++k)
  {
    f32 t = (f32)k/n;
    f32z v = f32z_exp(-f32z_I*F32_TAU*t) * out[k + n/2];
    f32z e = out[k];
    out[k] = e + v;
    out[k + n/2] = e - v;
  }
}

// post: pop off reverse preorder

#if 0
INTERNAL f32z *
fft_it(MemArena *arena, f32 *in, u32 n)
{
  f32z *out = MEM_ARENA_PUSH_ARRAY(arena, f32z, n);
  MemArenaTemp t = mem_arena_temp_begin(&arena, 1);

  mem_arena_temp_end(t);
}
#endif


INTERNAL f32 
f32z_power(f32z z)
{
  f32 mag = f32z_mag(z);
  f32 power = SQUARE(mag);
  // logarithmic compresses larger values, expands smaller values
  // lower frequencies dominating, so F32_LN(power)
  return F32_LN(power);
}

// as playing chiptune, getting square waves
// a square wave is composed of many sine waves, so will see ramp 

#if 0
INTERNAL MusicFile *
alloc_mf()
{
  MusicFile *mf = NULL;
  if (g_state->first_free_mf != NULL)
  {
    mf = g_state->first_free_mf;
    g_state->first_free_mf = g_state->first_free_mf->next;
  }
  else
  {
    mf = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->arena, MusicFile);
  }
  mf->gen++;

  return mf;
}

INTERNAL void
dealloc_mf(MusicFile *mf)
{
  DLL_REMOVE(g_state->first_mf, g_state->last_mf, mf);
  mf->next = g_state->first_free_mf;
  mf->prev = NULL;
  g_state->first_free_mf = mf;
  mf->gen++;
}

INTERNAL Handle 
handle_from_mf(MusicFile *mf)
{
  Handle handle = ZERO_STRUCT;
  if (mf != NULL)
  {
    handle.addr = mf;
    handle.gen = mf->gen;
  }
  return handle;
}

INTERNAL MusicFile *
mf_from_handle(Handle handle)
{
  MusicFile *result = (MusicFile *)handle.addr;
  if (result != NULL && handle.gen != result->gen)
  {
    result = NULL;
  }
  return result;
}
#endif

INTERNAL void 
music_callback(void *buffer, unsigned int frames)
{
  // NOTE(Ryan): Don't overwrite buffer on this run
  if (frames >= NUM_SAMPLES) frames = NUM_SAMPLES - 1;

  // NOTE(Ryan): Raylib normalises to f32 stereo for all sources
  f32 *norm_buf = (f32 *)buffer;
  for (u32 i = 0; i < frames * 2; i += 2)
  {
    f32 left = norm_buf[i];
    f32 right = norm_buf[i + 1];

    g_state->samples_ring.samples[g_state->samples_ring.head] = MAX(left, right);
    g_state->samples_ring.head = (g_state->samples_ring.head + 1) % NUM_SAMPLES;
  }
}

#define DRAW_U32(var) do {\
  String8 s = str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %" PRIu32, var); \
  char text[64] = ZERO_STRUCT; \
  str8_to_cstr(s, text, sizeof(text)); \
  DrawText(text, 50, 50, 48, RED); \
} while (0)

#define DRAW_F32(var) do {\
  String8 s = str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %f", var); \
  char text[64] = ZERO_STRUCT; \
  str8_to_cstr(s, text, sizeof(text)); \
  DrawText(text, 50, 50, 48, RED); \
} while (0)

INTERNAL void
fft_render(Rectangle r, f32 *samples, u32 num_samples)
{
  DrawRectangleRec(r, BLACK);

  f32 bin_w = r.width / num_samples;
  for (u32 i = 0; i < num_samples; i += 1)
  {
    f32 bin_h = samples[i] * r.height;
    Rectangle bin_rect = {
      r.x + (i * bin_w), 
      r.y + (r.height - bin_h), 
      bin_w, 
      bin_h
    }; 

    f32 hue = (f32)i / num_samples;
    Color c = ColorFromHSV(360 * hue, 1.0f, 1.0f);
    DrawRectangleRec(bin_rect, c);
  }
}

#if TEST_BUILD
int testable_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
  global_debugger_present = linux_was_launched_by_gdb();
  MemArena *arena = mem_arena_allocate(GB(8), MB(64));

  // String8List cmd_line = ZERO_STRUCT;
  // for (u32 i = 0; i < argc; i += 1)
  // {
  //   String8 arg = str8_cstr(argv[i]);
  //   str8_list_push(arena, &cmd_line, arg);
  // }

  ThreadContext tctx = thread_context_allocate(GB(8), MB(64));
  tctx.is_main_thread = true;
  thread_context_set(&tctx);
  thread_context_set_name("Main Thread");

#if RELEASE_BUILD
  linux_set_cwd_to_self();
#else
  linux_append_ldlibrary(str8_lit("./build"));
#endif

  profiler_init();

  SetTraceLogLevel(LOG_ERROR); 
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);

  u32 screen_width = 1080;
  u32 screen_height = 720;
  InitWindow(screen_width, screen_height, "Music Visualiser");
  SetTargetFPS(60);

  InitAudioDevice();
  
  // fc-list
  Font font = LoadFontEx("assets/Alegraya.ttf", 64, NULL, 0);

  g_state = MEM_ARENA_PUSH_STRUCT_ZERO(arena, State);
  g_state->arena = arena;
  g_state->frame_arena = mem_arena_allocate(GB(1), MB(64));

  for (b32 quit = false; !quit; g_state->frame_counter += 1)
  {  
    BeginDrawing();
    ClearBackground(RAYWHITE);

    f32 dt = GetFrameTime();
    u32 rw = GetRenderWidth();
    u32 rh = GetRenderHeight();
    f32 font_size = rh / 16;

    if (IsKeyPressed(KEY_F)) 
    {
      if (IsWindowMaximized()) RestoreWindow();
      else MaximizeWindow();
    }

    if (IsFileDropped())
    {
      FilePathList dropped_files = LoadDroppedFiles();
      char *path = dropped_files.paths[dropped_files.count-1];

      if (IsMusicReady(g_state->music))
      {
        StopMusicStream(g_state->music);
        DetachAudioStreamProcessor(g_state->music.stream, music_callback);
        UnloadMusicStream(g_state->music);
      }

      g_state->music = LoadMusicStream(path);
      AttachAudioStreamProcessor(g_state->music.stream, music_callback);
      //SetMusicVolume(g_state->music, 0.5f);
      PlayMusicStream(g_state->music);

      UnloadDroppedFiles(dropped_files);
    }

    if (IsKeyPressed(KEY_P))
    {
      if (IsMusicStreamPlaying(g_state->music)) PauseMusicStream(g_state->music);
      else ResumeMusicStream(g_state->music);
    }

    UpdateMusicStream(g_state->music); 

    //MusicFile *active_mf = mf_from_handle(g_state->active_mf);
    //if (active_mf != NULL)
    //{
    //  Music music = active_mf->music;
    //}

    if (!IsMusicReady(g_state->music))
    {
      // TODO(Ryan): Have tag that will not run CI for temp commits
      char *t = "Drag 'n' Drop Music";
      Vector2 ts = MeasureTextEx(font, t, font_size, 0);
      // font.baseSize
      Vector2 tv = {
        rw/2.0f - ts.x/2.0f,
        rh/2.0f - ts.y/2.0f
      };
      DrawTextEx(font, t, tv, font_size, 0, RED);
    }
    else
    {
      for (u32 i = 0, j = g_state->samples_ring.head; 
           i < NUM_SAMPLES; 
           i += 1, j = (j - 1) % NUM_SAMPLES)
      {
        // we are multiplying by 1Hz, so shifting frequencies.
        f32 t = (f32)i/(NUM_SAMPLES - 1);
        g_state->hann_samples[i] = hann_function(g_state->samples_ring.samples[j], t);
      }

      PROFILE_BANDWIDTH("fft", NUM_SAMPLES * sizeof(f32))
      {
        fft(g_state->hann_samples, 1, g_state->fft_samples, NUM_SAMPLES);
      }

      f32 max_power = 1.0f;
      // NOTE(Ryan): FFT is periodic, so only have to iterate half of fft samples
      for (u32 i = 0; i < HALF_SAMPLES; i += 1)
      {
        f32 power = f32z_power(g_state->fft_samples[i]);
        if (power > max_power) max_power = power;
      }

      u32 num_bins = 0;
      for (f32 f = 1.0f; (u32)f < HALF_SAMPLES; f = F32_CEIL(f * 1.06f))
      {
        num_bins += 1; 
      }

      u32 j = 0;
      for (f32 f = 1.0f; (u32)f < HALF_SAMPLES; f = F32_CEIL(f * 1.06f))
      {
        f32 next_f = F32_CEIL(f * 1.06f);
        f32 bin_power = 0.0f;
        for (u32 i = (u32)f; i < HALF_SAMPLES && i < (u32)next_f; i += 1)
        {
          f32 p = f32z_power(g_state->fft_samples[i]); 
          if (p > bin_power) bin_power = p;
        }

        // division by zero in float gives -nan
        f32 target_t = bin_power / max_power;
        g_state->draw_samples[j] += (target_t - g_state->draw_samples[j]) * 8 * dt;

        j += 1;
      }

      Rectangle fft_region = {0.0f, 0.0f, (f32)rw, (f32)rh};
      fft_render(fft_region, g_state->draw_samples, num_bins);
    }

    EndDrawing();

    quit = WindowShouldClose();
    #if ASAN_ENABLED
      if (GetTime() >= 5.0) quit = true;
    #endif
    mem_arena_clear(g_state->frame_arena);
  }
  CloseWindow();

  profiler_end_and_print();
  // PROFILE_BANDWIDTH(), PROFILE_BLOCK(), PROFILE_FUNCTION(), 

  // NOTE(Ryan): Run explicitly so as to not register a leak for arenas
  LSAN_RUN();
  return 0;
}

PROFILER_END_OF_COMPILATION_UNIT
