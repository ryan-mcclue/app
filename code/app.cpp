// SPDX-License-Identifier: zlib-acknowledgement
#if !TEST_BUILD
#define PROFILER 1
#endif

#include "base/base-inc.h"
#include <raylib.h>

#include "app.h"

#include <dlfcn.h>

GLOBAL void *g_app_reload_handle = NULL;
GLOBAL char g_nil_update_err_msg[128];

INTERNAL void
app_nil_update(State *state)
{
  BeginDrawing();
  ClearBackground(BLACK);

  char *err = dlerror();
  if (err != NULL)
  {
    strncpy(g_nil_update_err_msg, err, sizeof(g_nil_update_err_msg));
  }

  f32 font_size = 64.0f;
  Vector2 size = MeasureTextEx(state->font, g_nil_update_err_msg, font_size, 0);
  Vector2 pos = {
    GetRenderWidth()/2.0f - size.x/2.0f,
    GetRenderHeight()/2.0f - size.y/2.0f,
  };
  DrawTextEx(state->font, g_nil_update_err_msg, pos, font_size, 0, RED);

  EndDrawing();
}
INTERNAL void app_nil(State *s) {}
GLOBAL AppCode g_nil_app_code = {
  .preload = app_nil,
  .update = app_nil_update,
  .postload = app_nil
};

INTERNAL AppCode 
app_reload(void)
{
  if (g_app_reload_handle != NULL) dlclose(g_app_reload_handle);

  g_app_reload_handle = dlopen("build/app-reload-debug.so", RTLD_NOW);
  if (g_app_reload_handle == NULL) return g_nil_app_code;

  AppCode result = ZERO_STRUCT;
  void *name = dlsym(g_app_reload_handle, "app_preload");
  if (name == NULL) return g_nil_app_code;
  result.preload = (app_preload_t)name;

  name = dlsym(g_app_reload_handle, "app_update");
  if (name == NULL) return g_nil_app_code;
  result.update = (app_update_t)name;

  name = dlsym(g_app_reload_handle, "app_postload");
  if (name == NULL) return g_nil_app_code;
  result.postload = (app_postload_t)name;

  return result;
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

  State *state = MEM_ARENA_PUSH_STRUCT_ZERO(arena, State);
  state->arena = arena;
  state->frame_arena = mem_arena_allocate(GB(1), MB(64));

  // fc-list
  state->font = LoadFontEx("assets/Lato-Medium.ttf", 64, NULL, 0);

  for (u32 i = 0; i < MUSIC_FILE_POOL_SIZE; i += 1)
  {
     MusicFileIndex *mf_idx = &state->mf_idx_pool[i]; 
     mf_idx->index = i;
     SLL_STACK_PUSH(state->first_free_mf_idx, mf_idx);
  }

  Image fullscreen_btn_img = LoadImage("assets/fullscreen-icon.png");
  state->fullscreen_tex = LoadTextureFromImage(fullscreen_btn_img);
  // we will be scaling, so want anti-aliasing
  SetTextureFilter(state->fullscreen_tex, TEXTURE_FILTER_BILINEAR);

  AppCode app_code = app_reload();
  for (b32 quit = false; !quit; state->frame_counter += 1)
  {  
    if (IsKeyPressed(KEY_R))
    {
      app_code.preload(state);
      app_code = app_reload();
      app_code.postload(state);
    }

    app_code.update(state);

    quit = WindowShouldClose();
    #if ASAN_ENABLED
      if (GetTime() >= 5.0) quit = true;
    #endif
    mem_arena_clear(state->frame_arena);
  }
  CloseWindow();

  profiler_end_and_print();
  // PROFILE_BANDWIDTH(), PROFILE_BLOCK(), PROFILE_FUNCTION(), 

  // NOTE(Ryan): Run explicitly so as to not register a leak for arenas
  LSAN_RUN();
  return 0;
}

PROFILER_END_OF_COMPILATION_UNIT
