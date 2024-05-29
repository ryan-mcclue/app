// SPDX-License-Identifier: zlib-acknowledgement
#if !defined(TEST_BUILD)
#define PROFILER 1
#endif

#include "base/base-inc.h"

#include <raylib.h>

// size_t m = fft_analyze(GetFrameTime());

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


INTERNAL void
hann()
{
  for (u32 i = 0; i < SAMPLES; i += 1)
  {
    f32 t = (f32)i/(SAMPLES - 1);

    f32 hann = 0.5f - 0.5f * F32_COS(F32_TAU * t);

    in[i] * hann
  }
}

// fft(in, 1, out, N);
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

INTERNAL f32 
powz(f32z z)
{
  // logarithmic compresses larger, expands smaller
  // lower frequencies dominating, so log(amp)
  f32 mag = f32z_mag(z);
  f32 power = SQUARE(mag);
  return F32_LN(power);
}

// as playing chiptune, getting square waves
// a square wave is composed of many sine waves, so will see ramp 

#if 0
while() { 
    fft(in, 1, out, N);

    float max_amp = 0.0f;
    for (size_t i = 0; i < N; ++i) {
        float a = amp(out[i]);
        if (max_amp < a) max_amp = a;
    }

    float step = 1.06;
    size_t m = 0;
    for (float f = 20.0f; (size_t) f < N; f *= step) {
        m += 1;
    }

NOTE: only have to iterate over half as mirrored?
    float cell_width = (float)w/m;
    m = 0;
    for (float f = 20.0f; (size_t) f < N/2; f *= step) {
        float f1 = f*step;
        float a = 0.0f;
        for (size_t q = (size_t) f; q < N/2 && q < (size_t) f1; ++q) {
            a += amp(out[q]);
        }
        a /= (size_t) f1 - (size_t) f + 1;
        float t = a/max_amp;
        DrawRectangle(m*cell_width, h/2 - h/2*t, cell_width, h/2*t, BLUE);
        m += 1;
    }
}
#endif



#define GLOBAL_SAMPLE_SIZE 512
GLOBAL f32 global_samples_ping[GLOBAL_SAMPLE_SIZE];
GLOBAL f32 global_samples_pong[GLOBAL_SAMPLE_SIZE];
GLOBAL b32 global_callback_pinging;

GLOBAL f32 global_draw_samples[GLOBAL_SAMPLE_SIZE];

INTERNAL void 
music_callback(void *buffer, unsigned int frames)
{
  if (frames >= GLOBAL_SAMPLE_SIZE) frames = GLOBAL_SAMPLE_SIZE - 1;

  global_callback_pinging = !global_callback_pinging;

  f32 *active_buf = global_callback_pinging ? global_samples_ping : global_samples_pong;

  // NOTE(Ryan): Raylib normalises to f32 stereo for all sources
  f32 *norm_buf = (f32 *)buffer;
  for (u32 i = 0, j = 0; i < frames * 2; i += 2, j += 1)
  {
    // TODO(Ryan): Use max() as downsampling
    f32 left = norm_buf[i];
    f32 right = norm_buf[i + 1];
    f32 sample_avg = (left + right) / 2.0f;

    active_buf[j] = sample_avg;
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

  u32 screen_width = 1080;
  u32 screen_height = 720;
  InitWindow(screen_width, screen_height, "Music Visualiser");
  SetTargetFPS(60);

  InitAudioDevice();
  Music music = LoadMusicStream("assets/billys-sacrifice.mp3");
  //Music music = LoadMusicStream("assets/checking-manifest.mp3");
  // NOTE(Ryan): Must play initially to be able to resume
  PlayMusicStream(music);
  PauseMusicStream(music);
  AttachAudioStreamProcessor(music.stream, music_callback);
  //DetachAudioStreamProcessor(music.stream, music_callback);
  //  SetMusicVolume(plug->music, 0.5f);


  // Font font = LoadFontEx("./fonts/Alegreya-Regular.ttf", 69, NULL, 0);
  //    Vector2 size = MeasureTextEx(plug->font, label, plug->font.baseSize, 0);
  //    Vector2 position = {
  //        w/2 - size.x/2,
  //        h/2 - size.y/2,
  //    };
  //    DrawTextEx(plug->font, label, position, plug->font.baseSize, 0, color);

  MemArena *frame_arena = mem_arena_allocate(GB(1), MB(64));
  u64 frame_counter = 0;
  for (b32 quit = false; !quit; frame_counter += 1)
  {  
    BeginDrawing();
    ClearBackground(RAYWHITE);

    f32 dt = GetFrameTime();

    //String8 s = u32_to_str8(frame_arena, global_frame_count);
    //char text[10] = ZERO_STRUCT;
    //str8_to_cstr(s, text, sizeof(text));
    //DrawText(text, 50, 50, 48, RED);

    UpdateMusicStream(music); 

    if (IsKeyPressed(KEY_P))
    {
      if (IsMusicStreamPlaying(music)) PauseMusicStream(music);
      else ResumeMusicStream(music);
    }

    f32 *active_buf = global_callback_pinging ? global_samples_pong : global_samples_ping;
    f32 max_draw_sample = f32_neg_inf();
    for (u32 i = 0; i < GLOBAL_SAMPLE_SIZE; i += 1)
    {
      global_draw_samples[i] += (active_buf[i] - global_draw_samples[i]) * dt;

      if (global_draw_samples[i] > max_draw_sample) max_draw_sample = global_draw_samples[i];
    }

    // f32 bar_w = (f32)GetScreenWidth() / (f32)ARRAY_COUNT(global_samples);
    f32 bar_w = 4.0f;
    for (u32 i = 0; i < GLOBAL_SAMPLE_SIZE; i += 1)
    {
      // TODO(Ryan): If frames < GLOBAL_SAMPLE_SIZE, will be drawing zeroed bars
      // that are not part of actual waveform
      f32 t = global_draw_samples[i] / max_draw_sample;
      f32 bar_h = t * (f32)GetScreenHeight();
      Rectangle bar_rect = {i * bar_w, GetScreenHeight() - bar_h, bar_w, bar_h};
      DrawRectangleRec(bar_rect, RED);
    }

    EndDrawing();

    quit = WindowShouldClose();
    #if ASAN_ENABLED
      if (GetTime() >= 5.0) quit = true;
    #endif
    mem_arena_clear(frame_arena);
  }
  CloseWindow();

  profiler_end_and_print();
  // PROFILE_BANDWIDTH(), PROFILE_BLOCK(), PROFILE_FUNCTION(), 

  // NOTE(Ryan): Run explicitly so as to not register a leak for arenas
  LSAN_RUN();
  return 0;
}

PROFILER_END_OF_COMPILATION_UNIT
