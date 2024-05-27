// SPDX-License-Identifier: zlib-acknowledgement
#if !defined(TEST_BUILD)
#define PROFILER 1
#endif

#include "base/base-inc.h"

#include <raylib.h>

GLOBAL f32 global_samples[1024];
GLOBAL f32 global_draw_samples[1024];
GLOBAL u32 global_it;

INTERNAL void 
music_callback(void *buffer, unsigned int frames)
{
  // NOTE(Ryan): Raylib normalises to float stereo
  f32 *norm_buf = (f32 *)buffer;
  for (u32 i = 0; i < frames * 2; i += 2)
  {
    f32 left = norm_buf[i];
    f32 right = norm_buf[i + 1];
    f32 sample_avg = (left + right) / 2.0f;

    if (global_it >= ARRAY_COUNT(global_samples)) global_it = 0;
    global_samples[global_it++] = sample_avg;
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
  // MemArena *frame_arena = mem_arena_allocate(GB(8), MB(64));

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
  // NOTE(Ryan): Must play initially to be able to resume
  PlayMusicStream(music);
  PauseMusicStream(music);
  AttachAudioStreamProcessor(music.stream, music_callback);
  //DetachAudioStreamProcessor(music.stream, music_callback);

  u64 frame_counter = 0;
  for (b32 quit = false; !quit; frame_counter += 1)
  {  
    BeginDrawing();
    ClearBackground(RAYWHITE);

    f32 dt = GetFrameTime();

    UpdateMusicStream(music); 

    if (IsKeyPressed(KEY_P))
    {
      if (IsMusicStreamPlaying(music)) PauseMusicStream(music);
      else ResumeMusicStream(music);
    }

    f32 max_draw_sample = f32_neg_inf();
    for (u32 i = 0; i < ARRAY_COUNT(global_samples); i += 1)
    {
      global_draw_samples[i] += (global_samples[i] - global_draw_samples[i]) * dt;

      if (global_draw_samples[i] > max_draw_sample) max_draw_sample = global_draw_samples[i];
    }

    // f32 bar_w = (f32)GetScreenWidth() / (f32)ARRAY_COUNT(global_samples);
    f32 bar_w = 4.0f;
    for (u32 i = 0; i < ARRAY_COUNT(global_samples); i += 1)
    {
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
  }
  CloseWindow();

  profiler_end_and_print();
  // PROFILE_BANDWIDTH(), PROFILE_BLOCK(), PROFILE_FUNCTION(), 

  // NOTE(Ryan): Run explicitly so as to not register a leak for arenas
  LSAN_RUN();
  return 0;
}

PROFILER_END_OF_COMPILATION_UNIT
