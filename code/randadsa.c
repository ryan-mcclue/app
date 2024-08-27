// SPDX-License-Identifier: zlib-acknowledgement
#if !TEST_BUILD
#define PROFILER 1
#endif

#include "desktop.h"

State *g_state = NULL;

#define COLOR_BG {120, 200, 22, 255}
#define BUTTON_MARGIN 0.05f
#define BUTTON_COLOR {200, 100, 10, 255}
#define BUTTON_COLOR_HOVEROVER ColorBrightness(BUTTON_COLOR, 0.15)
#define TOOLTIP_COLOR_BG {0, 50, 200, 255}
#define TOOLTIP_COLOR_FG {230, 230, 230, 255}

GLOBAL u64 g_active_button_id;

#include "desktop-assets.cpp"

INTERNAL Rectangle
cut_rect_left(Rectangle rect, f32 t)
{
  Rectangle r = rect;
  r.width *= t;
  return r;
}
INTERNAL Rectangle
cut_rect_right(Rectangle rect, f32 t)
{
  Rectangle r = rect;
  r.x += (r.width * t);
  r.width *= (1.f - t);
  return r;
}
INTERNAL Rectangle
cut_rect_vert_middle(Rectangle rect, f32 t0, f32 t1)
{
  Rectangle r = rect;
  r.x += (r.width * t0);
  r.width = (r.width * t1) - (r.width * t0);
  return r;
}
INTERNAL Rectangle
cut_rect_top(Rectangle rect, f32 t)
{
  Rectangle r = rect;
  r.height *= t;
  return r;
}
INTERNAL Rectangle
cut_rect_bottom(Rectangle rect, f32 t)
{
  Rectangle r = rect;
  r.y += (r.height * t);
  r.height *= (1.f - t);
  return r;
}
INTERNAL Vector2
snap_line_inside_other(Vector2 a, Vector2 b)
{
  Vector2 result = {b.x, b.y};

  f32 dt = b.y - b.x;
  if (result.x > a.y || result.y > a.y)
  {
    result.x = a.y - dt; 
    result.y = a.y;
  }

  if (result.x < a.x || result.y < a.x)
  {
    result.x = b.x;
    result.y = b.x + dt;
  }

  return result;
}
INTERNAL Rectangle 
snap_rect_inside_render(Rectangle boundary)
{
  Vector2 x = snap_line_inside_other({0, GetRenderWidth()}, {boundary.x, boundary.x + boundary.width});
  Vector2 y = snap_line_inside_other({0, GetRenderHeight()}, {boundary.y, boundary.y + boundary.height});

  f32 w = x.y - x.x;
  f32 h = y.y - y.x;
  return {x.x, y.x, w, h};
}
INTERNAL Rectangle 
align_rect(Rectangle target, Vector2 size, RECT_ALIGN side)
{
  Rectangle result = {target.x, target.y, size.x, size.y};
  switch (side)
  {
    default: { return result; }
    case RA_BOTTOM:
    {
      f32 x = target.x + target.width*.5f;
      f32 y = target.y + target.height;
      result.x = x - size.x*.5f;
      result.y = y;
    } break;
    case RA_TOP:
    {
      f32 x = target.x + target.width*.5f;
      f32 y = target.y - size.y;
      result.x = x - size.x*.5f;
      result.y = y;
    } break;
    case RA_RIGHT:
    {
      f32 x = target.x + target.width;
      f32 y = target.y + target.height*.5f;
      result.x = x;
      result.y = y - size.y*.5f;
    } break;
    case RA_LEFT:
    {
      f32 x = target.x - size.x;
      f32 y = target.y + target.height*.5f;
      result.x = x;
      result.y = y - size.y*.5f;
    } break;
    case RA_CENTRE:
    {
      f32 x = target.x + target.width*.5f - size.x*.5f;
      f32 y = target.y + target.height*.5f - size.y*.5f;
      result.x = x;
      result.y = y;
    } break;
  }
  return result;
}

INTERNAL void
push_z_layer(Z_LAYER z)
{
  ZLayerNode *n = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, ZLayerNode);
  n->value = z;
  SLL_STACK_PUSH(g_state->z_layer_stack, n);
}

INTERNAL void
pop_z_layer(void)
{
  SLL_STACK_POP(g_state->z_layer_stack);
}

#define Z_SCOPE(z) \
  DEFER_LOOP(push_z_layer(z), pop_z_layer())

INTERNAL void
push_rect(Rectangle r, Color c)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_RECT;
  re->z = g_state->z_layer_stack->value;
  re->colour = c;

  re->rec = r;
  SLL_STACK_PUSH(g_state->render_element_stack, re);
  g_state->render_element_stack_count += 1;
}

INTERNAL void
push_text(const char *s, Font f, f32 font_size, Vector2 p, Color c)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_TEXT;
  re->z = g_state->z_layer_stack->value;
  re->colour = c;

  re->font = f;
  re->font_size = font_size;
  re->rec = {p.x, p.y, 0.f, 0.f};
  re->text = MEM_ARENA_PUSH_ARRAY(g_state->frame_arena, char, strlen(s) + 1);
  MEMORY_COPY(re->text, s, strlen(s)+1);
  SLL_STACK_PUSH(g_state->render_element_stack, re);
  g_state->render_element_stack_count += 1;
}

INTERNAL void
push_texture(Texture t, Vector2 p, f32 scale, Color c)
{

}

INTERNAL void
push_circle(void)
{
  // specify segments to get a 'cleaner' circle than default
  //DrawCircleSector(cc, cc.x*.2f, 0, 360, 69, RED);
  //DrawCircleSector(cc, cc.x*.18f, 0, 360, 30, WHITE);
}

INTERNAL int
render_element_quick_sort_compare(RenderSortElement *a, RenderSortElement *b)
{
  return (a->z - b->z);
}

INTERNAL void
render_elements(void)
{
  RenderSortElement *sort_array = MEM_ARENA_PUSH_ARRAY(g_state->frame_arena, RenderSortElement, g_state->render_element_stack_count);
  u32 j = 0;
  for (RenderElement *re = g_state->render_element_stack; re != NULL; re = re->next)
  {
    RenderSortElement *s = &sort_array[j];
    s->z = re->z;
    s->element = re;
    j += 1;
  }

  QUICK_SORT(sort_array, RenderSortElement, g_state->render_element_stack_count, render_element_quick_sort_compare);

  for (u32 i = 0; i < g_state->render_element_stack_count; i += 1)
  {
    RenderSortElement *rs = &sort_array[i];
    RenderElement *re = rs->element;
    switch (re->type)
    {
      default: {} break;
      case RE_RECT:
      {
        DrawRectangleRec(re->rec, re->colour);
      } break;
      case RE_TEXT:
      {
        DrawTextEx(re->font, re->text, {re->rec.x, re->rec.y}, re->font_size, 0.f, re->colour);
      } break;
/*       case RE_TEXTURE:
      {
        DrawTextureEx();
      } break; */
    }
  }

  g_state->render_element_stack = NULL;
  g_state->render_element_stack_count = 0;
  g_state->z_layer_stack = NULL;
}

INTERNAL b32
consume_hover(Rectangle r)
{
  if (g_state->hover_consumed) return false;
  else return (g_state->hover_consumed = CheckCollisionPointRec(GetMousePosition(), r));
}

INTERNAL b32
consume_left_click(void)
{
  if (g_state->left_click_consumed) return false;
  else return (g_state->left_click_consumed = IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
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

EXPORT void 
code_preload(State *state)
{
  MusicFile *active = DEREF_MUSIC_FILE_HANDLE(state->active_music_handle);
  if (!ZERO_MUSIC_FILE(active)) 
  {
    DetachAudioStreamProcessor(active->music.stream, music_callback);
  }

  profiler_init();
  
  assets_preload(state);
}

EXPORT void 
code_postload(State *state)
{
  MusicFile *active = DEREF_MUSIC_FILE_HANDLE(state->active_music_handle);
  if (!ZERO_MUSIC_FILE(active)) 
  {
    AttachAudioStreamProcessor(active->music.stream, music_callback);
  }
}

EXPORT void
code_profiler_end_and_print(State *state)
{
  profiler_end_and_print();
}

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

/* INTERNAL void
mf_render(Rectangle r)
{
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
  f32 max_scroll = (mf_width * 10) - r.width;
  if (max_scroll < 0) max_scroll = (mf_width * 10);
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
} */

typedef struct ButtonState ButtonState;
struct ButtonState
{
  b32 clicked;
}; 

// also, hash(base_id, &i, sizeof(i))

// IMPORTANT: to overcome collisions, 
// have an optional parameter id to be passed in to base hash off  

INTERNAL void 
draw_tooltip_on_hover(Rectangle boundary, const char *text, RECT_ALIGN align)
{
  if (!consume_hover(boundary)) return;

  f32 font_size = g_state->font.baseSize * 0.75f;
  Vector2 text_size = MeasureTextEx(g_state->font, text, font_size, 0.f);
  Vector2 margin = {font_size*0.5f, font_size*0.1f};

  Vector2 size = {text_size.x + margin.x*2.f, text_size.y + margin.y*2.f};
  Rectangle tooltip_boundary = align_rect(boundary, size, align);
  Rectangle tooltip_rect = snap_rect_inside_render(tooltip_boundary);

  Z_SCOPE(Z_LAYER_TOOLTIP_BG)
  {
    // DrawRectangleRounded(tooltip_rect, 0.4, 20, TOOLTIP_COLOR_BG);
    push_rect(tooltip_rect, COLOR_BG);
    Z_SCOPE(Z_LAYER_TOOLTIP_FG)
    {
      Vector2 position = {tooltip_rect.x + tooltip_rect.width * .5f - text_size.x * .5f,
                          tooltip_rect.y + tooltip_rect.height * .5f - text_size.y * .5f};
      push_text(text, g_state->font, font_size, position, TOOLTIP_COLOR_FG);
    }
  }
}

INTERNAL ButtonState
draw_button_with_id(u64 id, Rectangle boundary)
{
  Vector2 mouse = GetMousePosition();
  b32 hovering = CheckCollisionPointRec(mouse, boundary);
  b32 clicked = false;

  // all inactive
  if (g_active_button_id == 0 && hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
  {
    g_active_button_id = id;
  }
  // we are active
  else if (g_active_button_id == id)
  {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
      g_active_button_id = 0;
      if (hovering)
      {
        clicked = true;
      }
    }
  }

  DrawRectangleRec(boundary, BUTTON_COLOR);

  return {clicked};
}

#define draw_button(boundary) \
  draw_button_with_location(__FILE__, __LINE__, boundary)

INTERNAL ButtonState
draw_button_with_location(char *file, u32 line, Rectangle boundary)
{
  u64 seed = hash_ptr(file);
  u64 id = hash_data(seed, &line, sizeof(line));

  draw_tooltip_on_hover(boundary, "Render [RR]", RA_TOP);

  return draw_button_with_id(id, boundary);
}


// state->red_slider = draw_slider(r, "Red: ", state->red_slider);
// state->red = draw_slider(r, "Red: ", state->red, &state->red_dragging)


/* 
INTERNAL COLOUR_PICKER_MODE
draw_mode()
{
  // char *hex_text = TextFormat("%02x%02x%02x%02x", value);
  // SetClipboardText(hex_text)
  // mode += 1 if LEFT_DOWN, -1 if RIGHT_DOWN over status
}


INTERNAL f32
draw_slider(Rectangle boundary, const char *label, f32 value, b32 *dragging)
{
  Rectangle lr = boundary, sr = boundary, vr = boundary;
  char *value_text = TextFormat("%0.02f", value);

  // ColorFromNormalized(state->red_slider.value, state->green)

  return value;
} */

INTERNAL void
draw_volume_slider(Rectangle boundary)
{
  Vector2 mouse_pos = GetMousePosition();
  // TODO: draw a single button; the slider is next to it on hover
  f32 base_w = boundary.width * 0.2f;
  f32 h = boundary.height * 0.8f;
  Rectangle slider_r = {boundary.x + boundary.x * BUTTON_MARGIN, 
                        boundary.y + boundary.y * BUTTON_MARGIN, 
                        base_w, h};

  LOCAL_PERSIST b32 expanded = false;
  LOCAL_PERSIST f32 value = 0.f;
  LOCAL_PERSIST b32 dragging = false;

  Color c = BUTTON_COLOR;
  expanded = dragging || CheckCollisionPointRec(mouse_pos, slider_r);
  if (expanded)
  {
    slider_r.width *= 5.f;
    c = BUTTON_COLOR_HOVEROVER;
  }
  DrawRectangleRounded(slider_r, 0.5, 20, c);

  if (expanded)
  {
    // draw horizontal slider
    Rectangle r = {slider_r.x + slider_r.x*BUTTON_MARGIN,
                   slider_r.y, 
                   slider_r.width - slider_r.width * 0.2f,
                   slider_r.height};
    Vector2 slider_start = {r.x, r.y + r.height*.5f}; 
    Vector2 slider_end = {r.x + r.width, r.y + r.height*.5f};
    DrawLineEx(slider_start, slider_end, r.height*0.15f, GREEN);

    f32 radius = r.height*0.25f;
    Vector2 slider_circle =  {slider_start.x + (slider_end.x - slider_start.x)*value, slider_start.y};
    DrawCircleV(slider_circle, radius, GREEN);

    b32 slider_circle_hover = CheckCollisionPointCircle(mouse_pos, slider_circle, radius);
    if (!dragging)
    {
      if (slider_circle_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      {
        dragging = true; 
      }
    }
    else
    {
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) dragging = false;

      //f32 value = GetMasterVolume();

      f32 x = CLAMP(slider_start.x, mouse_pos.x, slider_end.x);
      x -= slider_start.x;
      x /= (slider_end.x - slider_start.x);
      value = x;

      f32 wheel = GetMouseWheelMove();
      value += (4.f * wheel / boundary.width);
      value = CLAMP01(value);

      // SetMasterVolume()
    }
  }
}

INTERNAL MusicFile *
alloc_music_file(void)
{
  for (u32 i = 0; i < ARRAY_COUNT(g_state->music_files); i += 1)
  {
    MusicFile *m = &g_state->music_files[i];
    if (m->is_active) continue;

    m->is_active = true;
    m->gen += 1;

    return m;
  }

  ASSERT("Out of music file memory" && false);

  return &g_zero_music_file;
}

INTERNAL void
dealloc_music_file(MusicFile *m)
{
  m->gen += 1;
  m->is_active = false;
}


INTERNAL void
draw_scrollbar(Rectangle r)
{
  for (u32 i = 0; i < g_state->num_music_files; i += 1)
  {
    
  }
// IMPORTANT: GetCollisionRec(panel, item)
// this computes the intersection of two rectangles
// useful for BeginScissorMode()
}

INTERNAL void
draw_fft(Rectangle r, f32 *samples, u32 num_samples)
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


EXPORT void 
code_update(State *state)
{ 
  PROFILE_FUNCTION() {
  g_state = state;

  f32 dt = GetFrameTime();
  u32 rw = GetRenderWidth();
  u32 rh = GetRenderHeight();

  state->font = assets_get_font(str8_lit("assets/Alegreya-Regular.ttf"));
  push_z_layer(Z_LAYER_NIL);

  if (!state->is_initialised)
  {
    state->is_initialised = true;
  }

  if (IsKeyPressed(KEY_F)) 
  {
    if (IsWindowMaximized()) RestoreWindow();
    else MaximizeWindow();
  }

  BeginDrawing();
  ClearBackground(COLOR_BG);

  if (Vector2Length(GetMouseDelta()) > 0.0f)
  {
    g_state->mouse_last_moved_time = GetTime();
  }

  // :load music
  if (IsFileDropped())
  {
    FilePathList dropped_files = LoadDroppedFiles();
    for (u32 i = 0; i < dropped_files.count; i += 1)
    {
      char *path = dropped_files.paths[i];
      Music music = LoadMusicStream(path);
      if (!IsMusicReady(music)) WARN("Can't load music file %s\n", path);
      else
      {
        MusicFile *m = alloc_music_file();
        m->music = music;
        if (i == 0)
        {
          MusicFile *active = DEREF_MUSIC_FILE_HANDLE(state->active_music_handle);
          if (!ZERO_MUSIC_FILE(active))
          {
            StopMusicStream(active->music);
            DetachAudioStreamProcessor(active->music.stream, music_callback);
          }
          state->active_music_handle = TO_HANDLE(m);

          AttachAudioStreamProcessor(m->music.stream, music_callback);
          SetMusicVolume(m->music, 0.5f);
          PlayMusicStream(m->music);
        }
      }
    }
    UnloadDroppedFiles(dropped_files);
  }

  // :update music
  MusicFile *active = DEREF_MUSIC_FILE_HANDLE(state->active_music_handle);
  if (IsKeyPressed(KEY_P))
  {
    if (IsMusicStreamPlaying(active->music)) PauseMusicStream(active->music);
    else ResumeMusicStream(active->music);
  }
  else if (IsKeyPressed(KEY_C))
  {
    StopMusicStream(active->music);
    PlayMusicStream(active->music);
  }
  UpdateMusicStream(active->music);

  // :fft music
  for (u32 i = 0, j = state->samples_ring.head; 
       i < NUM_SAMPLES; 
       i += 1, j = (j - 1) % NUM_SAMPLES)
  {
    // we are multiplying by 1Hz, so shifting frequencies.
    f32 t = (f32)i / (NUM_SAMPLES - 1);
    state->hann_samples[i] = hann_function(state->samples_ring.samples[j], t);
  }

  fft(state->hann_samples, 1, state->fft_samples, NUM_SAMPLES);

  f32 max_power = 1.0f;
  for (u32 i = 0; i < HALF_SAMPLES; i += 1)
  {
    f32 power = F32_LN(f32z_power(state->fft_samples[i]));
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
      f32 p = F32_LN(f32z_power(state->fft_samples[i]));
      if (p > bin_power) bin_power = p;
    }

    f32 target_t = bin_power / max_power;
    state->draw_samples[j] += (target_t - state->draw_samples[j]) * 8 * dt;

    j += 1;
  }

  Rectangle render_region = {0.f, 0.f, (f32)rw, (f32)rh};
  f32 color_region_t = 0.7f;
  Rectangle color_region = cut_rect_bottom(render_region, color_region_t);

  Rectangle top_region = cut_rect_top(render_region, color_region_t);
  f32 scroll_region_t = 0.2f;
  Rectangle scroll_region = cut_rect_left(top_region, scroll_region_t);
  Rectangle fft_region = cut_rect_right(top_region, scroll_region_t);

  draw_scrollbar(scroll_region);
  draw_fft(fft_region, state->draw_samples, num_bins);
  draw_color_picker(color_region);
/*
  Rectangle slider_region = {rw*.2f, rh*.2f, rw*.5f, rh*.1f};
  DrawRectangleRec(slider_region, {255, 10, 20, 255});
  draw_volume_slider(slider_region);

  Rectangle r = {800, 800, 200, 200};
  draw_button(r); */

  render_elements();

  g_state->hover_consumed = false;
  g_state->left_click_consumed = false;

  g_dbg_at_y = 0.f;
  EndDrawing();
  }
}

PROFILER_END_OF_COMPILATION_UNIT

#if !defined(DESKTOP_H)
#define DESKTOP_H

#include "base/base-inc.h"
#include "desktop-assets.h"
#include <raylib.h>
#include <raymath.h>

#define V2(x, y) CCOMPOUND(Vector2){(f32)x, (f32)y}
#if defined(LANG_CPP)
INTERNAL Vector2 operator*(Vector2 a, Vector2 b) { return Vector2Multiply(a, b); }
INTERNAL Vector2 operator*(f32 s, Vector2 a) { return Vector2Scale(a, s); }
INTERNAL Vector2 operator*(Vector2 a, f32 s) { return Vector2Scale(a, s); }
INTERNAL Vector2 & operator*=(Vector2 &a, f32 s) { a = a * s; return a; } 

INTERNAL Vector2 operator+(f32 b, Vector2 a) { return Vector2AddValue(a, b); }
INTERNAL Vector2 operator+(Vector2 a, f32 b) { return Vector2AddValue(a, b); }
INTERNAL Vector2 operator+(Vector2 a, Vector2 b) { return Vector2Add(a, b); }
INTERNAL Vector2 & operator+=(Vector2 &a, Vector2 b) { a = a + b; return a; }

INTERNAL Vector2 operator-(f32 b, Vector2 a) { return Vector2SubtractValue(a, b); }
INTERNAL Vector2 operator-(Vector2 a, f32 b) { return Vector2SubtractValue(a, b); }
INTERNAL Vector2 operator-(Vector2 a, Vector2 b) { return Vector2Subtract(a, b); }
INTERNAL Vector2 & operator-=(Vector2 &a, Vector2 b) { a = a - b; return a; }
INTERNAL Vector2 operator-(Vector2 a) { return Vector2Negate(a); }

INTERNAL bool operator==(Vector2 a, Vector2 b) { return f32_eq(a.x, b.x) && f32_eq(a.y, b.y); }
INTERNAL bool operator!=(Vector2 a, Vector2 b) { return !(a == b); }
#endif

typedef struct MusicFile MusicFile;
struct MusicFile
{
  bool is_active;
  Music music;
  u64 gen;
};
GLOBAL MusicFile g_zero_music_file;
#define DEREF_MUSIC_FILE_HANDLE(h) \
  (((h.addr) == NULL || (h.gen != ((MusicFile *)(h.addr))->gen)) ? &g_zero_music_file : (MusicFile *)(h.addr))
#define ZERO_MUSIC_FILE(ptr) \
  (ptr == &g_zero_music_file) 
#define MAX_MUSIC_FILES 64

// IMPORTANT(Ryan): The number of samples limits number of frequencies we can derive
// This seems to be a good number for a decent visualisation
// Also, as displaying logarithmically, we don't actually have this large number
#define NUM_SAMPLES (1 << 13) 
STATIC_ASSERT(IS_POW2(NUM_SAMPLES));
#define HALF_SAMPLES (NUM_SAMPLES >> 1)
struct SampleRing
{
  f32 samples[NUM_SAMPLES];
  u32 head;
};

typedef enum
{
  RA_NIL = 0,
  RA_TOP,
  RA_RIGHT,
  RA_BOTTOM,
  RA_LEFT,
  RA_CENTRE
} RECT_ALIGN;

typedef enum
{
  RE_NIL = 0,
  RE_RECT,
  RE_TEXT,
  RE_TEXTURE,
  RE_CIRCLE
} RENDER_ELEMENT_TYPE;

typedef enum
{
  Z_LAYER_NIL = 0,
  Z_LAYER_FFT,
  Z_LAYER_TOOLTIP_BG,
  Z_LAYER_TOOLTIP_FG,
} Z_LAYER;

typedef struct RenderElement RenderElement;
struct RenderElement
{
  RENDER_ELEMENT_TYPE type;

  RenderElement *next;

  Z_LAYER z;
  Color colour;

  Rectangle rec;

  Font font;
  f32 font_size;
  char *text;

  Texture texture;
  f32 scale;
};


typedef struct RenderSortElement RenderSortElement;
struct RenderSortElement
{
  Z_LAYER z;
  RenderElement *element;
};

typedef struct ZLayerNode ZLayerNode; 
struct ZLayerNode
{
  ZLayerNode *next;
  Z_LAYER value;
};

typedef struct State State;
INTROSPECT() struct State
{
  b32 is_initialised;

  Assets assets;

  MemArena *arena;
  MemArena *frame_arena;
  u64 frame_counter;

  Font font;

  b32 hover_consumed;
  b32 left_click_consumed;

  ZLayerNode *z_layer_stack;
  RenderElement *render_element_stack;
  u32 render_element_stack_count;

  MusicFile music_files[MAX_MUSIC_FILES];
  Handle active_music_handle;

  SampleRing samples_ring;
  f32 hann_samples[NUM_SAMPLES];
  f32z fft_samples[NUM_SAMPLES];
  f32 draw_samples[HALF_SAMPLES];

  f32 mouse_last_moved_time;
  f32 mf_scroll_velocity;
  f32 mf_scroll;
};

typedef void (*code_preload_t)(State *s);
typedef void (*code_update_t)(State *s);
typedef void (*code_postload_t)(State *s);
typedef void (*code_profiler_end_and_print_t)(State *s);

typedef struct ReloadCode ReloadCode;
struct ReloadCode
{
  code_preload_t preload;
  code_update_t update;
  code_postload_t postload;
  code_profiler_end_and_print_t profiler_end_and_print;
};

GLOBAL f32 g_dbg_at_y;
extern State *g_state; 
INTERNAL void
draw_debug_text(String8 s)
{
  char text[64] = ZERO_STRUCT;
  str8_to_cstr(s, text, sizeof(text));
  DrawText(text, 50.f, g_dbg_at_y, 48, RED);
  g_dbg_at_y += 50.f;
}
#if DEBUG_BUILD
#define DBG_U32(var) \
  draw_debug_text(str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %" PRIu32, var))
#define DBG_S32(var) \
  draw_debug_text(str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %" PRId32, var))
#define DBG_U64(var) \
  draw_debug_text(str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %" PRIu64, var))
#define DBG_S64(var) \
  draw_debug_text(str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %" PRId64, var))
#define DBG_F32(var) \
  draw_debug_text(str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %f", var))
#define DBG_F64(var) \
  draw_debug_text(str8_fmt(g_state->frame_arena, STRINGIFY(var) " = %lf", var))
#define DBG_V2(var) \
  draw_debug_text(str8_fmt(g_state->frame_arena, STRINGIFY(var) " = (%f, %f)", var.x, var.y))
#else
#define DBG_U32(var)
#define DBG_S32(var)
#define DBG_U64(var)
#define DBG_S64(var)
#define DBG_F32(var)
#define DBG_F64(var)
#define DBG_V2(var)
#endif


#endif

