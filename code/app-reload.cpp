// SPDX-License-Identifier: zlib-acknowledgement
#include "base/base-inc.h"
#include <raylib.h>
#include <raymath.h>
#include "app.h"

GLOBAL State *g_state = NULL;

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
#endif

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
  if (result == NULL || handle.gen != result->gen)
  {
    result = &g_zero_mf;
  }
  return result;
}

INTERNAL b32
mf_is_zero(MusicFile *mf)
{
  return mf == &g_zero_mf;
}

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

    // float thick = cell_width/3*sqrtf(t);
    // DrawLineEx(startPos, endPos, thick, color);
  }
}

INTERNAL Color
lerp_color(Color *a, Color *b, f32 t)
{
  Color res = {
    (u8)f32_lerp(a->r, b->r, t),
    (u8)f32_lerp(a->g, b->g, t),
    (u8)f32_lerp(a->b, b->b, t),
    (u8)f32_lerp(a->a, b->a, t),
  };

  return res;
}

#define ASSETS_NUM_SLOTS 256

// to write generic code in C, just focus on ptr and bytes (void* to u8*)
// same structure, different ending members
// require offsetof for padding

INTERNAL void *
assets_find_(void *slots, String8 key, memory_index slot_size, 
            memory_index key_offset,
            memory_index next_offset, 
            memory_index value_offset)
{
  u64 hash = str8_hash(key);
  u64 slot_i = hash % ASSETS_NUM_SLOTS;
  u8 *slot = (u8 *)slots + slot_i * slot_size; 

  u8 *first_node = slot;
  String8 node_key = *(String8 *)(first_node + key_offset);
  if (str8_match(node_key, key, 0)) return (first_node + value_offset);

  for (u8 *chain_node = (first_node + next_offset);
       chain_node != NULL; 
       chain_node = (chain_node + next_offset))
  {
    node_key = *(String8 *)(chain_node + key_offset);
    if (str8_match(node_key, key, 0)) return (u8 *)(chain_node + value_offset);
  }

  return NULL;
}

// IMPORTANT(Ryan): View macro parameters as tokens to be expanded, e.g. could be any number of values
// (u8 *)(&slots[0].first->hash_next) - (u8 *)(&slots[0])
// NOTE(Ryan): gf2 (ctrl-d for assembly)
// with optimisations, compiler will determine offsets at compile time
#define ASSETS_FIND(slots, key) \
  assets_find_(slots, key, sizeof(*slots), \
              OFFSET_OF_MEMBER(__typeof__(*slots[0].first), key), \
              OFFSET_OF_MEMBER(__typeof__(*slots[0].first), hash_chain_next), \
              OFFSET_OF_MEMBER(__typeof__(*slots[0].first), value))

INTERNAL Font
assets_get_font(String8 key)
{
  Font *p = (Font *)ASSETS_FIND(g_state->assets.fonts.slots, key);
  if (p != NULL) return *p;
  else
  {
    char cpath[256] = ZERO_STRUCT;
    str8_to_cstr(key, cpath, sizeof(cpath)); 

    // TODO(Ryan): Add parameters to asset keys
    Font v = LoadFontEx(cpath, 64, NULL, 0);

    FontNode *n = MEM_ARENA_PUSH_STRUCT(g_state->assets.arena, FontNode);
    n->key = key;
    n->value = v;

    u64 slot_i = str8_hash(key) % ASSETS_NUM_SLOTS;
    FontSlot *s = &g_state->assets.fonts.slots[slot_i];
    __SLL_QUEUE_PUSH(s->first, s->last, n, hash_chain_next);
    __SLL_STACK_PUSH(g_state->assets.fonts.collection, n, hash_collection_next);

    return v;
  }
}

INTERNAL Image
assets_get_image(String8 key)
{
  Image *p = (Image *)ASSETS_FIND(g_state->assets.images.slots, key);
  if (p != NULL) return *p;
  else
  {
    char cpath[256] = ZERO_STRUCT;
    str8_to_cstr(key, cpath, sizeof(cpath)); 

    Image v = LoadImage(cpath);

    ImageNode *n = MEM_ARENA_PUSH_STRUCT(g_state->assets.arena, ImageNode);
    n->key = key;
    n->value = v;

    u64 slot_i = str8_hash(key) % ASSETS_NUM_SLOTS;
    ImageSlot *s = &g_state->assets.images.slots[slot_i];
    __SLL_QUEUE_PUSH(s->first, s->last, n, hash_chain_next);
    __SLL_STACK_PUSH(g_state->assets.images.collection, n, hash_collection_next);

    return v;
  }
}

INTERNAL Texture
assets_get_texture(String8 key)
{
  Texture *p = (Texture *)ASSETS_FIND(g_state->assets.textures.slots, key);
  if (p != NULL) return *p;
  else
  {
    Image i = assets_get_image(key);
    Texture v = LoadTextureFromImage(i);
    SetTextureFilter(v, TEXTURE_FILTER_BILINEAR);

    TextureNode *n = MEM_ARENA_PUSH_STRUCT(g_state->assets.arena, TextureNode);
    n->key = key;
    n->value = v;

    u64 slot_i = str8_hash(key) % ASSETS_NUM_SLOTS;
    TextureSlot *s = &g_state->assets.textures.slots[slot_i];
    __SLL_QUEUE_PUSH(s->first, s->last, n, hash_chain_next);
    __SLL_STACK_PUSH(g_state->assets.textures.collection, n, hash_collection_next);

    return v;
  }
}

INTERNAL void
assets_preload(void)
{
#define X(Name, name, names) \
  for (Name##Node *n = g_state->assets.names.collection; \
       n != NULL; \
       n = n->hash_collection_next) \
  { \
    Unload##Name(n->value); \
  } \
  g_state->assets.names = ZERO_STRUCT;

  ASSETS_STRUCTS_LIST
#undef X

  mem_arena_clear(g_state->assets.arena);
#define X(Name, name, names) \
  g_state->assets.names.slots = MEM_ARENA_PUSH_ARRAY_ZERO(g_state->assets.arena, Name##Slot, ASSETS_NUM_SLOTS);

  ASSETS_STRUCTS_LIST
#undef X
}


INTERNAL void
mf_render(Rectangle r)
{
  // timeline panel
  // GetMusicTimePlayed()/GetMusicTimeLength() 
  // f32 t = (mouse_x - bar.x) / r.w; 
  // f32 play_pos = t * GetMusicTimeLength(); 
  // SeekMusicStream(music, play_pos);

  // SDF for font good at any size
  // GetFileName()
  
  // vertical scroll: Begin/EndScissorMode();

  // TODO: cut out overflowed text
  // TODO: add padding when drawing UI
  // TODO: color selection on brightness  

  // if (entire_scroll_area > visible_area)
  // {
  //   f32 t = visible_area / entire_scroll_area;
  // }
  // scroll is part of region. so substract it when drawing items
  // f32 scroll_height = r.height * 0.1f;
  // Rectangle scroll_bar_region = {
  //   mf_region.x + mf_scroll,
  //   mf_region.y + mf_region.height - scroll_height,
  //   mf_region.width * t,
  //   scroll_height
  // };
  // f32 scroll_bar_off = (mf_scroll / entire_scroll) * w;

  Color bg_color = DARKGRAY;
  DrawRectangleRec(r, bg_color);

  f32 mf_dim = r.height * 0.7f;
  f32 mf_margin = r.height * 0.1f; 
  f32 mf_width = mf_dim;
  f32 mf_height = mf_width;
  u32 i = 0;

  f32 mf_v = mf_width;
  g_state->mf_scroll_velocity *= 0.9f;
  g_state->mf_scroll_velocity += GetMouseWheelMove() * mf_v;
  g_state->mf_scroll += g_state->mf_scroll_velocity * GetFrameTime(); 

  f32 max_scroll = (mf_width * g_state->num_mf) - r.width;
  if (max_scroll < 0) max_scroll = (mf_width * g_state->num_mf);
  g_state->mf_scroll = CLAMP(0.0f, g_state->mf_scroll, max_scroll);

  MusicFile *active_mf = mf_from_handle(g_state->active_mf_handle);
  for (MusicFile *mf = g_state->first_mf;
       mf != NULL; mf = mf->next)
  {
    Rectangle mf_rec = {g_state->mf_scroll + r.x + mf_margin + i * (mf_margin + mf_width), 
                        r.y + mf_margin, 
                        mf_width, 
                        mf_height};
    Color mf_color = YELLOW;
    
    if (mf == active_mf)
    {
      mf_color = PURPLE;
    }
    if (CheckCollisionPointRec(GetMousePosition(), mf_rec))
    {
      // TODO: add filename tooltip
      mf_color = GREEN;
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
      {
        if (!mf_is_zero(active_mf))
        {
          StopMusicStream(active_mf->music);
          DetachAudioStreamProcessor(active_mf->music.stream, music_callback);
        }
        AttachAudioStreamProcessor(mf->music.stream, music_callback);
        SetMusicVolume(mf->music, 0.5f);
        PlayMusicStream(mf->music);
        g_state->active_mf_handle = handle_from_mf(mf);
      }
    }
    DrawRectangleRec(mf_rec, mf_color);

    i += 1;
  }
}

INTERNAL bool
draw_fullscreen_btn(Rectangle region)
{
  bool clicked = false;

  f64 delta = (GetTime() - g_state->mouse_last_moved_counter);
  bool draw_fullscreen = (g_state->fullscreen_fft && delta < 3.0f) || 
                         !g_state->fullscreen_fft;
  if (draw_fullscreen) 
  {
    Color btn_color = GRAY;
    bool hover_over = false;
    if (CheckCollisionPointRec(GetMousePosition(), region))
    {
      hover_over = true;
      btn_color = ColorBrightness(GRAY, 0.15); // -0.15 makes darker
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
      {
        clicked = true;
      } 
    }

    u32 icon_index = 0;
    // NOTE: boolean to indexes, perhaps: i = (hover_over << 1) | (fullscreen)
    if (g_state->fullscreen_fft)
    {
      if (hover_over)
      {
        icon_index = 3;
      }
      else
      {
        icon_index = 2;
      }
    }
    else
    {
      if (hover_over)
      {
        icon_index = 0;
      }
      else
      {
        icon_index = 1;
      }
    }

    DrawRectangleRounded(region, 0.5f, 20.0f, btn_color);

    f32 icon_size = 225.0f;
    f32 btn_scale = (region.width / icon_size) * 0.75f;
    Rectangle src = {icon_index * 225.f, icon_index * 225.f, 225.f, 225.f};
    Rectangle dst = {
      region.x + region.width/2.0f - (icon_size/2.0f)*btn_scale,
      region.y + region.height/2.0f - (icon_size/2.0f)*btn_scale,
      icon_size*btn_scale,
      icon_size*btn_scale
    };
    Vector2 origin = {0.f, 0.f};
    DrawTexturePro(assets_get_texture(str8_lit("assets/fullscreen-icon.png")),
                   src, dst, origin, 0.f, ColorBrightness(WHITE, -0.1f)); 
    // make texture white to tint to all
    //DrawTextureEx(fullscreen_btn_tex, btn_pos, 0.0f, btn_scale, ColorBrightness(WHITE, -0.1f)); 
  }

  return clicked;
}


EXPORT void 
app_preload(State *state)
{
  MusicFile *active_mf = mf_from_handle(state->active_mf_handle);
  DetachAudioStreamProcessor(active_mf->music.stream, music_callback);

  assets_preload();
}

EXPORT void 
app_postload(State *state)
{
  MusicFile *active_mf = mf_from_handle(state->active_mf_handle);
  AttachAudioStreamProcessor(active_mf->music.stream, music_callback);
}

EXPORT void 
app_update(State *state)
{
  g_state = state;

  BeginDrawing();
  ClearBackground(BLACK);

  f32 dt = GetFrameTime();
  u32 rw = GetRenderWidth();
  u32 rh = GetRenderHeight();
  f32 font_size = rh / 8;

  Font main_font = assets_get_font(str8_lit("assets/Lato-Medium.ttf"));

  if (IsKeyPressed(KEY_F)) 
  {
    if (IsWindowMaximized()) RestoreWindow();
    else MaximizeWindow();
  }

  if (Vector2Length(GetMouseDelta()) > 0.0f)
    //if (mouse_delta.x > 0.0f || mouse_delta.y > 0.0f)
  {
    g_state->mouse_last_moved_counter = GetTime();
  }

  if (IsFileDropped())
  {
    FilePathList dropped_files = LoadDroppedFiles();
    for (u32 i = 0; i < dropped_files.count; i += 1)
    {
      MusicFileIndex *mf_idx = g_state->first_free_mf_idx;
      if (mf_idx != NULL)
      {
        MusicFile *mf = &g_state->mf_pool[mf_idx->index];
        char *path = dropped_files.paths[i];
        mf->music = LoadMusicStream(path);
        if (!IsMusicReady(mf->music))
        {
          // TODO: display as popup 
          TraceLog(LOG_ERROR, "Can't load music file %s\n", path);
        }
        else
        {
          SLL_STACK_POP(g_state->first_free_mf_idx);
          SLL_QUEUE_PUSH(g_state->first_mf, g_state->last_mf, mf);
          mf->gen = 1;
          g_state->num_mf += 1;
        }
      }
    }

    if (g_state->last_mf != NULL)
    {
      MusicFile *active_mf = mf_from_handle(g_state->active_mf_handle);
      // IMPORTANT: get a feel for when to use assert()
      // it's for programmer error, i.e. we stuffed up; not the library etc.
      ASSERT(active_mf != NULL);
      if (!mf_is_zero(active_mf))
      {
        StopMusicStream(active_mf->music);
        DetachAudioStreamProcessor(active_mf->music.stream, music_callback);
        // UnloadMusicStream(active_mf->music);
      }

      active_mf = g_state->last_mf; 
      g_state->active_mf_handle = handle_from_mf(active_mf);

      AttachAudioStreamProcessor(active_mf->music.stream, music_callback);
      SetMusicVolume(active_mf->music, 0.5f);
      PlayMusicStream(active_mf->music);
    }

    // IMPORTANT: no float equality, e.g. f==0 (f > F32_EPSILON)

    UnloadDroppedFiles(dropped_files);
  }


  MusicFile *active_mf = mf_from_handle(g_state->active_mf_handle);

  if (IsKeyPressed(KEY_P))
  {
    if (IsMusicStreamPlaying(active_mf->music)) PauseMusicStream(active_mf->music);
    else ResumeMusicStream(active_mf->music);
  }
  else if (IsKeyPressed(KEY_C))
  {
    StopMusicStream(active_mf->music);
    PlayMusicStream(active_mf->music);
  }

  UpdateMusicStream(active_mf->music); 

  if (!IsMusicReady(active_mf->music))
  {
    char *text = "Drag 'n' Drop Music";

    Color backing_colour = {255, 173, 0, 255};

    Color front_start_colour = WHITE;
    Color front_end_colour = {255, 222, 0, 255}; 

    f32 t_base = 0.4f;
    f32 t_range = 0.54f;

    f64 delta = (GetTime() - g_state->mouse_last_moved_counter);

    f32 flash_duration = 1.0f; // 1 second
    f32 s = delta / flash_duration;
    s = SQUARE(s); // F32_SQRT(F32_ABS(s)); linear to hump
    s = 1 - s; // ease-out
    if (delta < flash_duration)
    {
      Color new_front_start_colour = {255, 0, 255, 255};
      front_start_colour = lerp_color(&front_start_colour, &new_front_start_colour, s);

      Color new_front_end_colour = {255, 0, 0, 255};
      front_end_colour = lerp_color(&front_end_colour, &new_front_end_colour, s);

      t_range += (0.2f - t_range) * s; 
    }

    f32 t = F32_COS(g_state->frame_counter * 3 * dt);
    t *= t;
    t = t_base + t_range * t;
    Color front_colour = lerp_color(&front_start_colour, &front_end_colour, t);

    u32 offset = main_font.baseSize / 40;

    Vector2 t_size = MeasureTextEx(main_font, text, font_size, 0);
    Vector2 t_vec = {
      rw/2.0f - t_size.x/2.0f,
      rh/2.0f - t_size.y/2.0f
    };
    Vector2 b_vec = {
      t_vec.x + offset,
      t_vec.y - offset,
    };

    DrawTextEx(main_font, text, t_vec, font_size, 0, backing_colour);
    DrawTextEx(main_font, text, b_vec, font_size, 0, front_colour);
  }
  else
  {
    if (IsKeyPressed(KEY_SPACE)) {
      g_state->fullscreen_fft ^= 1;
    }

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

    f32 fft_h = (f32)rh * 0.75f;
    if (g_state->fullscreen_fft)
    {
      fft_h = (f32)rh;
    }

    Rectangle fft_region = {0.0f, 0.0f, (f32)rw, fft_h};
    fft_render(fft_region, g_state->draw_samples, num_bins);

    Rectangle mf_region = {fft_region.x, fft_region.y + fft_region.height,
      fft_region.width, (f32)rh - fft_region.height};
    mf_render(mf_region);

    f32 btn_w = 100.f;
    f32 btn_margin = 10.f;
    Rectangle btn_rec = {fft_region.x + fft_region.width - btn_w - btn_margin, 
      fft_region.y + btn_margin, btn_w, btn_w};
    if (draw_fullscreen_btn(btn_rec))
    {
      g_state->fullscreen_fft ^= 1;
    }
  }

  EndDrawing();
}

