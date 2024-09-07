// SPDX-License-Identifier: zlib-acknowledgement
#if !TEST_BUILD
#define PROFILER 1
#endif

#include "app.h"

State *g_state = NULL;

#define COLOR_BG0 GetColor(0xfdf6e3ff)
#define COLOR_BG1 GetColor(0xeee8d5ff)
#define COLOR_FONT GetColor(0x657b83ff)
#define COLOR_YELLOW_ACCENT GetColor(0xb58900ff)
#define COLOR_ORANGE_ACCENT GetColor(0xcb4b16ff)
#define COLOR_RED_ACCENT GetColor(0xdc322fff)
#define COLOR_MAGENTA_ACCENT GetColor(0xd33682ff)
#define COLOR_VIOLET_ACCENT GetColor(0x6c71c4ff)
#define COLOR_BLUE_ACCENT GetColor(0x268bd2ff)
#define COLOR_CYAN_ACCENT GetColor(0x2aa198ff)
#define COLOR_GREEN_ACCENT GetColor(0x859900ff)

#define BUTTON_MARGIN 0.05f
#define BUTTON_COLOR {200, 100, 10, 255}
#define BUTTON_COLOR_HOVEROVER ColorBrightness(BUTTON_COLOR, 0.15)
#define TOOLTIP_COLOR_BG {0, 50, 200, 255}
#define TOOLTIP_COLOR_FG {230, 230, 230, 255}

GLOBAL u64 g_active_button_id;

#include "app-assets.cpp"

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
snap_rect_inside_render(Rectangle region)
{
  Vector2 x = snap_line_inside_other({0, GetRenderWidth()}, {region.x, region.x + region.width});
  Vector2 y = snap_line_inside_other({0, GetRenderHeight()}, {region.y, region.y + region.height});

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
    case RA_LEFT_INNER:
    {
      result.x = target.x + target.width * 0.02f;
      result.y = target.y + target.height*.5f - size.y*.5f;;
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
push_alpha(f32 a)
{
  AlphaNode *n = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, AlphaNode);
  n->value = a;
  SLL_STACK_PUSH(g_state->alpha_stack, n);
}
INTERNAL void
pop_alpha(void)
{
  SLL_STACK_POP(g_state->alpha_stack);
}
#define ALPHA_SCOPE(a) \
  DEFER_LOOP(push_alpha(a), pop_alpha())

INTERNAL void
push_mouse_cursor(MouseCursor m)
{
  MouseCursorNode *n = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, MouseCursorNode);
  n->value = m;
  SLL_STACK_PUSH(g_state->mouse_cursor_stack, n);
}
INTERNAL void
pop_mouse_cursor(void)
{
  SLL_STACK_POP(g_state->mouse_cursor_stack);
}
#define MOUSE_CURSOR_SCOPE(m) \
  DEFER_LOOP(push_mouse_cursor(m), pop_mouse_cursor())


INTERNAL void
push_rect(Rectangle r, Color c, f32 roundness = 0.f, int segments = 0)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_RECT;
  re->z = g_state->z_layer_stack->value;
  re->colour = {c.r, c.g, c.b, c.a * g_state->alpha_stack->value};

  re->rec = r;
  re->roundness = roundness;
  re->segments = segments;
  SLL_QUEUE_PUSH(g_state->render_element_first, g_state->render_element_last, re);
  g_state->render_element_queue_count += 1;
}

INTERNAL void
push_rect_outline(Rectangle r, Color c, f32 thickness, f32 roundness = 0.f, int segments = 0)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_RECT_OUTLINE;
  re->z = g_state->z_layer_stack->value;
  re->colour = {c.r, c.g, c.b, c.a * g_state->alpha_stack->value};

  re->rec = r;
  re->thickness = thickness;
  re->roundness = roundness;
  re->segments = segments;
  SLL_QUEUE_PUSH(g_state->render_element_first, g_state->render_element_last, re);
  g_state->render_element_queue_count += 1;
}

INTERNAL void
push_text(const char *s, Font f, f32 font_size, Vector2 p, Color c)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_TEXT;
  re->z = g_state->z_layer_stack->value;
  re->colour = {c.r, c.g, c.b, c.a * g_state->alpha_stack->value};

  re->font = f;
  re->font_size = font_size;
  re->rec = {p.x, p.y, 0.f, 0.f};
  re->text = MEM_ARENA_PUSH_ARRAY(g_state->frame_arena, char, strlen(s) + 1);
  MEMORY_COPY(re->text, s, strlen(s)+1);
  SLL_QUEUE_PUSH(g_state->render_element_first, g_state->render_element_last, re);
  g_state->render_element_queue_count += 1;
}

INTERNAL void
push_circle(Vector2 p, f32 radius, Color c)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_CIRCLE;
  re->z = g_state->z_layer_stack->value;
  re->colour = {c.r, c.g, c.b, c.a * g_state->alpha_stack->value};

  re->rec = {p.x, p.y, 0.f, 0.f};
  re->radius = radius;
  SLL_QUEUE_PUSH(g_state->render_element_first, g_state->render_element_last, re);
  g_state->render_element_queue_count += 1;
}

INTERNAL void
push_line(Vector2 start, Vector2 end, f32 thickness, Color c)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_LINE;
  re->z = g_state->z_layer_stack->value;
  re->colour = {c.r, c.g, c.b, c.a * g_state->alpha_stack->value};

  re->rec = {start.x, start.y, end.x, end.y};
  re->thickness = thickness;
  SLL_QUEUE_PUSH(g_state->render_element_first, g_state->render_element_last, re);
  g_state->render_element_queue_count += 1;
}


INTERNAL void
push_texture(Texture t, Vector2 p, f32 scale, Color c)
{

}


INTERNAL void
merge_sort_render_elements(RenderSortElement *entries, u32 count, RenderSortElement *merge_temp) 
{
  if (count == 1) return;
  else if (count == 2 && (entries[0].z > entries[1].z))
  {
    SWAP(RenderSortElement, entries[0], entries[1]);
  }
  else
  {
    u32 half0 = count / 2;
    u32 half1 = count - half0;

    merge_sort_render_elements(entries, half0, merge_temp);
    merge_sort_render_elements(entries + half0, half1, merge_temp);

    RenderSortElement *start_half0 = entries;
    RenderSortElement *start_half1 = entries + half0;
    RenderSortElement *end = entries + count;

    RenderSortElement *read_half0 = start_half0;
    RenderSortElement *read_half1 = start_half1;
    RenderSortElement *out = merge_temp;
    for (u32 i = 0; i < count; i += 1)
    {
      if (read_half0 == start_half1)
      {
        *out++ = *read_half1++;   
      }
      else if (read_half1 == end)
      {
        *out++ = *read_half0++;
      }
      else if (read_half0->z <= read_half1->z)
      {
        *out++ = *read_half0++;
      }
      else
      {
        *out++ = *read_half1++;
      }
    }

    // write back
    for (u32 i = 0; i < count; i += 1)
    {
      entries[i] = merge_temp[i];
    }
  }
}

INTERNAL void
render_elements(void)
{
  SetMouseCursor(g_state->mouse_cursor_stack->value);

  RenderSortElement *sort_array = MEM_ARENA_PUSH_ARRAY(g_state->frame_arena, RenderSortElement, g_state->render_element_queue_count);
  RenderSortElement *sort_array_temp = MEM_ARENA_PUSH_ARRAY(g_state->frame_arena, RenderSortElement, g_state->render_element_queue_count);
  u32 j = 0;
  for (RenderElement *re = g_state->render_element_first; re != NULL; re = re->next)
  {
    RenderSortElement *s = &sort_array[j];
    s->z = re->z;
    s->element = re;
    j += 1;
  }

  merge_sort_render_elements(sort_array, g_state->render_element_queue_count, sort_array_temp);

  for (u32 i = 0; i < g_state->render_element_queue_count; i += 1)
  {
    RenderSortElement *rs = &sort_array[i];
    RenderElement *re = rs->element;
    switch (re->type)
    {
      default: { ASSERT("Drawing nil type" && 0); } break;
      case RE_RECT:
      {
        DrawRectangleRounded(re->rec, re->roundness, re->segments, re->colour);
      } break;
      case RE_RECT_OUTLINE:
      {
        DrawRectangleRoundedLines(re->rec, re->roundness, re->segments, re->thickness, re->colour);
      } break;
      case RE_TEXT:
      {
        DrawTextEx(re->font, re->text, {re->rec.x, re->rec.y}, re->font_size, 0.f, re->colour);
      } break;
      case RE_CIRCLE:
      {
        DrawCircleV({re->rec.x, re->rec.y}, re->radius, re->colour);
      } break;
      case RE_LINE:
      {
        DrawLineEx({re->rec.x, re->rec.y}, {re->rec.width, re->rec.height}, re->thickness, re->colour);
      } break;
    }
  }

  g_state->render_element_first = NULL;
  g_state->render_element_last = NULL;
  g_state->render_element_queue_count = 0;
  g_state->z_layer_stack = NULL;
  g_state->alpha_stack = NULL;
  g_state->mouse_cursor_stack = NULL;
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

typedef enum 
{
  BS_NIL = 0,
  BS_HOVERING = 1 << 0,
  BS_CLICKED = 1 << 1,
} BUTTON_STATE; 

// also, hash(base_id, &i, sizeof(i))

// IMPORTANT: to overcome collisions, 
// have an optional parameter id to be passed in to base hash off  

INTERNAL void 
draw_tooltip(Rectangle region, const char *text, RECT_ALIGN align)
{
  f32 font_size = g_state->font.baseSize * 0.75f;
  Vector2 text_size = MeasureTextEx(g_state->font, text, font_size, 0.f);
  Vector2 margin = {font_size*0.5f, font_size*0.1f};

  Vector2 size = {text_size.x + margin.x*2.f, text_size.y + margin.y*2.f};
  Rectangle tooltip_boundary = align_rect(region, size, align);
  Rectangle tooltip_rect = snap_rect_inside_render(tooltip_boundary);

  Z_SCOPE(Z_LAYER_TOOLTIP)
  {
    // DrawRectangleRounded(tooltip_rect, 0.4, 20, TOOLTIP_COLOR_BG);
    push_rect(tooltip_rect, COLOR_BG0);
    Vector2 position = {tooltip_rect.x + tooltip_rect.width * .5f - text_size.x * .5f,
                        tooltip_rect.y + tooltip_rect.height * .5f - text_size.y * .5f};
    push_text(text, g_state->font, font_size, position, TOOLTIP_COLOR_FG);
  }
}

INTERNAL void
push_rect_with_label(Rectangle r, const char *label, Color c, RECT_ALIGN text_align = RA_CENTRE)
{
  f32 font_size = r.height * 0.45f;
  Vector2 label_dim = MeasureTextEx(g_state->font, label, font_size, 0.f);
  Vector2 label_margin = {font_size*0.5f, font_size*0.1f};
  Vector2 label_size = {label_dim.x + label_margin.x*2.f, label_dim.y + label_margin.y*2.f};
  Rectangle label_rect = align_rect(r, label_size, text_align);
  push_rect(r, c);
  push_rect_outline(r, BLACK, 5.0f);

  Vector2 label_pos = {label_rect.x, label_rect.y};
  f32 offset = (font_size / 40.f);
  Vector2 label_shadow = {label_pos.x + offset, label_pos.y + offset}; 
  push_text(label, g_state->font, font_size, label_shadow, BLACK);
  push_text(label, g_state->font, font_size, label_pos, WHITE);
}

INTERNAL BUTTON_STATE
draw_button_with_id(u64 id, Rectangle region)
{
  Vector2 mouse = GetMousePosition();
  b32 hovering = CheckCollisionPointRec(mouse, region);
  b32 clicked = false;

  if (g_state->active_button_id == 0 && hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
  {
    g_state->active_button_id = id;
  }
  else if (g_state->active_button_id == id)
  {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
      g_state->active_button_id = 0;
      if (hovering)
      {
        clicked = true;
      }
    }
  }

  u32 mask = !!hovering | (!!clicked << 1);
  return (BUTTON_STATE)mask;
}

#define draw_button(region, name) \
  draw_button_with_location(__LINE__, name, region)

INTERNAL BUTTON_STATE
draw_button_with_location(u32 line, const char *name, Rectangle region)
{
  u64 seed = hash_ptr(name);
  u64 id = hash_data(seed, &line, sizeof(line));

  return draw_button_with_id(id, region);
}

// other state would be: b32 *expanded
INTERNAL void
draw_slider(Rectangle region, f32 *value, b32 *dragging)
{
  Vector2 mouse_pos = GetMousePosition();

  Vector2 padding = {region.width * 0.05f, region.height * 0.4f};
  f32 w = region.width - 2.f*padding.x;
  f32 h = region.height - 2.f*padding.y;
  Rectangle bar_r = {region.x + padding.x, region.y + padding.y, w, h};
  push_rect(bar_r, COLOR_ORANGE_ACCENT, 0.5, 20);
  //push_rect(bar_r, COLOR_ORANGE_ACCENT);

  f32 radius = region.height*0.25f;
  f32 offset = bar_r.width*(*value) + radius;
  f32 circle_x = bar_r.x + offset;
  Vector2 circle = {bar_r.x + offset, bar_r.y + bar_r.height*.5f};
  push_circle(circle, radius, COLOR_ORANGE_ACCENT);

  b32 hovering = CheckCollisionPointCircle(mouse_pos, circle, radius);
  if (hovering || *dragging)
  {
    push_mouse_cursor(MOUSE_CURSOR_RESIZE_EW);
  }

  if (!*dragging )
  {
    if (hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
      *dragging = true;
    }
  }
  else
  {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) *dragging = false;

    f32 x = CLAMP(bar_r.x, mouse_pos.x, bar_r.x + bar_r.width - radius);
    x -= bar_r.x;
    x /= bar_r.width;
    *value = x;
/*     f32 wheel = GetMouseWheelMove();
    value += (4.f * wheel / region.width);
    value = CLAMP01(value); */
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
draw_scroll_region(Rectangle r)
{
  push_rect(r, COLOR_BG1);

  f32 btn_padding = r.width * 0.05f;
  f32 btn_w = r.width - btn_padding*2.f;
  f32 btn_h = r.height * 0.1f;

  f32 scroll_w = r.width * 0.1f;
  f32 scrollable_area = (btn_h + 2.f*btn_padding) * g_state->num_loaded_music_files;

  if (scrollable_area > r.height)
  {
    btn_w -= scroll_w;

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, r))
    {
      g_state->scroll_velocity -= GetMouseWheelMove() * btn_h * 8.f;
    }
    g_state->scroll_velocity *= 0.9f;
    g_state->scroll += g_state->scroll_velocity * GetFrameTime();

    if (g_state->scroll < 0) g_state->scroll = 0;
    f32 max_scroll = scrollable_area - r.height;
    if (g_state->scroll > max_scroll) g_state->scroll = max_scroll;

    Rectangle scroll_bg = {r.x + r.width - scroll_w, r.y, scroll_w, r.height};
    push_rect(scroll_bg, COLOR_BLUE_ACCENT);

    f32 scroll_off_t = g_state->scroll / scrollable_area;
    f32 scroll_h_t = r.height / scrollable_area;
    f32 scroll_bar_padding = scroll_w * 0.1f;
    f32 scroll_bar_w = scroll_w - 2.f*scroll_bar_padding;
    Rectangle scroll_fg = {scroll_bg.x + scroll_bar_padding, r.y + r.height * scroll_off_t,
                           scroll_bar_w, r.height * scroll_h_t};
    push_rect(scroll_fg, COLOR_CYAN_ACCENT);
  }
  
  // push_start_scissor(r); BeginScissorMode(r.x, r.y, r.width, r.height);
  // push_end_scissor(); EndScissorMode();

  MusicFile *active = DEREF_MUSIC_FILE_HANDLE(g_state->active_music_handle);
  for (u32 i = 0; i < ARRAY_COUNT(g_state->music_files); i += 1)
  {
    MusicFile *m = &g_state->music_files[i];
    if (!m->is_active) continue;

    Color c = COLOR_BLUE_ACCENT;
    if (m == active) 
    {
      c = COLOR_CYAN_ACCENT;
    }

    Rectangle btn_r = {r.x + btn_padding, 
                       r.y + btn_padding + i*(btn_h + btn_padding) - g_state->scroll,
                       btn_w, btn_h};
    
    BUTTON_STATE bs = draw_button(btn_r, m->file_name);

    if (bs & (BS_CLICKED | BS_HOVERING))
    {
      push_mouse_cursor(MOUSE_CURSOR_POINTING_HAND);
    }
    if (bs & BS_CLICKED)
    {
      StopMusicStream(active->music);
      DetachAudioStreamProcessor(active->music.stream, music_callback);

      g_state->active_music_handle = TO_HANDLE(m);

      AttachAudioStreamProcessor(m->music.stream, music_callback);
      SetMusicVolume(m->music, 0.5f);
      PlayMusicStream(m->music);
    } 
    else if (bs & BS_HOVERING)
    {
      String8 s = str8_fmt(g_state->frame_arena, "Length: %us", (u32)GetMusicTimeLength(m->music));
      draw_tooltip(btn_r, (const char *)s.content, RA_RIGHT);
      c = ColorBrightness(c, 0.1);
    }

    push_rect_with_label(btn_r, m->file_name, c);

  }
}
// IMPORTANT: GetCollisionRec(panel, item)
// this computes the intersection of two rectangles
// useful for BeginScissorMode()

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

/* INTERNAL
void draw_text_input(Rectangle r)
{
  // TODO: selection: engine programming 5-6
  BUTTON_STATE bs = draw_button(r, "text-input");
  if (bs & (BS_CLICKED | BS_HOVERING))
  {
    push_mouse_cursor(MOUSE_CURSOR_IBEAM);
  }
  if (bs & BS_CLICKED)
  {
    Vector2 mouse = GetMousePosition();
    g_state->text_input_cursor_t = (mouse.x - r.x) / r.width;
    if (!g_state->text_input_active)
    {
      g_state->text_input_buffer_len = snprintf(g_state->text_input_buffer, ARRAY_COUNT(g_state->text_input_buffer), 
                                                "%u", g_state->text_input_val);
    }
    g_state->text_input_active = true;

    Vector2 text_dim = MeasureTextEx(g_state->font, g_state->text_input_buffer, size, 0.f);
    f32 ch_width = text_dim.x / strlen(g_state->text_input_buffer);
    u32 at_estimate = (g_state->text_input_cursor_t * r.width) / ch_width;
    g_state->text_input_buffer_at = at_estimate;
  }
  if (g_state->text_input_active)
  {
    handle_text_input_u32(r, &g_state->text_input_val); 
  }
  else
  {
    String8 s = str8_fmt(g_state->frame_arena, "%u", g_state->text_input_val);
    push_rect_with_label(r, (const char *)s.content, COLOR_GREEN_ACCENT, RA_LEFT_INNER);
  }
}

INTERNAL void
handle_text_input_u32(Rectangle r, u32 *value)
{
  // computing cursor distance from glyph positions for cursor movement
  // so, no rescaled text, this way can just iterate through font glyphs dimensions

  if (!*dragging )
  {
    if (hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
      *dragging = true;

      selection_info.min = index_point;
      selection_info.max = index_point;
    }
  }
  else
  {
    selection.max = index_point;
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) *dragging = false;
    g_state->text_input_cursor_t = (mouse.x - r.x) / r.width;
  }

  if (IsKeyPressed(KEY_ENTER))
  {
    g_state->text_input_active = false; 
    // parse_and_store_input_text();
    // expression parser: jon-blow operator precedence + haversine parser
    *value = (u32)str8_to_int(g_state->text_input_buffer);
  }
  if ((IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) && 
       g_state->text_input_buffer_at > 0)
  {
    g_state->text_input_buffer_at -= 1;
  }
  if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) && 
       g_state->text_input_buffer_at > 0)
  {
    g_state->text_input_buffer_at -= 1;
  }
  if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && 
       g_state->text_input_buffer_at > 0)
  {
    g_state->text_input_buffer_at -= 1;
    MEMORY_COPY(g_state->text_input_buffer + g_state->text_input_buffer_at + 1, 
                g_state->text_input_buffer + g_state->text_input_buffer_at,
                buf_len - g_state->text_input_buffer_at);
  }

  char ch = GetCharPressed();
  while (ch > 0)
  {
    u32 buf_max = ARRAY_COUNT(g_state->text_input_buffer);
    if (g_state->text_input_buffer_len == buf_max - 1)
    {
      g_state->text_input_buffer[buf_max - 1] = '\0';
      break;
    }
    MEMORY_COPY(input_buf + input_cursor, 
                input_buf + input_cursor + 1,
                buf_len - input_cursor + 1);
    input_buf[input_cursor++] = ch;
    g_state->text_input_buffer_len++;
    ch = GetCharPressed();
  }

  // draw rect base

  // draw selection
  Color selection_color = BLACK;
  u32 min = MIN(selection.min, selection.max);
  u32 max = MAX(selection.min, selection.max);
  f32 width = (max - min) * ch_width;
  Rectangle s_rect = {r.x + selection.min * ch_width, r.y, width, height};

  // draw text
  // DrawTextEx(g_state->input_buffer, left_align_text_pos)

  // draw cursor
  // TODO: compute accurate cursor position
  Rectangle source_rect = g_state->input_r;
  // ch_width = text_len / input_cur_len;
  // cursor_x = source_rect.x + (cursor_p * input_cur_len + 0.5f) * ch_width;
  // cursor_w = (buffer_at == buffer_len ? 20 : 5);
  f32 cursor_x = source_rect.x + g_state->input_cursor_p * g_state->input_text_len + 0.5f;
  cursor_x = CLAMP(0, cursor_x, ARRAY_COUNT(input_text_bufffer));

  Rectangle cursor_r = { cursor_x, source_rect.y + source_rect.height * 0.05f, // something with text height
                       20.f, text_height};
  // cursor_color = lerp(cursor_c, button_bg, t);
  push_rect(cursor_r, cursor_color);

} */

INTERNAL void
draw_correlation_region(Rectangle r, f32 correlation)
{
  push_rect(r, COLOR_BG1);

  char *label = "Music Correlation:";
  f32 font_size = g_state->font.baseSize * 2.f;
  Vector2 text_size = MeasureTextEx(g_state->font, label, font_size, 0.f);
  Vector2 margin = {font_size * 0.5f, font_size * 0.1f};
  Vector2 size = {text_size.x + margin.x * 2.f, text_size.y + margin.y * 2.f};
  Rectangle rect = align_rect(r, size, RA_CENTRE);
  push_text(label, g_state->font, font_size, {rect.x, rect.y}, COLOR_FONT);

  const char *text = "";
  Color c = ZERO_STRUCT;
  if (correlation < 0.25f)
  {
    text = "low"; c = COLOR_GREEN_ACCENT;
  }
  else if (correlation > 0.75f)
  {
    text = "high"; c = COLOR_RED_ACCENT;
  }
  else
  {
    text = "medium"; c = COLOR_YELLOW_ACCENT;
  }
  push_text(text, g_state->font, font_size, {rect.x + rect.width + 5.f, rect.y}, c);
  // if (mouse_released_over_field) draw_text_input(r, INPUT_RED_COMPONENT, red_width)
}

INTERNAL void
draw_fft(Rectangle r, f32 *samples, u32 num_samples)
{
  // TODO: volume slider
  push_rect(r, COLOR_BG0);

  Rectangle play_slider = cut_rect_bottom(r, 0.9f);
  r = cut_rect_top(r, 0.9f);
  r.x += 5.0f;

  MusicFile *active = DEREF_MUSIC_FILE_HANDLE(g_state->active_music_handle);
  if (!ZERO_MUSIC_FILE(active))
  {
    f32 music_length = GetMusicTimeLength(active->music);
    f32 prev_slider = GetMusicTimePlayed(active->music) / music_length;
    g_state->active_music_slider_value = prev_slider;
    draw_slider(play_slider, &g_state->active_music_slider_value, &g_state->active_music_slider_dragging);
    if (!f32_eq(g_state->active_music_slider_value, prev_slider))
    {
      SeekMusicStream(active->music, music_length * g_state->active_music_slider_value);
    }

    if (g_state->is_music_paused)
    {
      char *text = "PAUSED";
      f32 font_size = g_state->font.baseSize * 1.5f;
      Vector2 text_size = MeasureTextEx(g_state->font, text, font_size, 0.f);
      Vector2 margin = {font_size * 0.5f, font_size * 0.1f};

      Vector2 size = {text_size.x + margin.x * 2.f, text_size.y + margin.y * 2.f};
      Rectangle tooltip_boundary = align_rect(r, size, RA_RIGHT);
      Rectangle tooltip_rect = snap_rect_inside_render(tooltip_boundary);
      push_text(text, g_state->font, font_size, {tooltip_rect.x, tooltip_rect.y}, COLOR_MAGENTA_ACCENT);
    }
  }

  f32 bin_w = 0.f;
  if (!f32_eq(num_samples, 0.f)) bin_w = (r.width / num_samples);

  for (u32 i = 0; i < num_samples; i += 1)
  {
    f32 bin_h = samples[i] * r.height;
    f32 thickness = bin_w / (3.f * F32_SQRT(samples[i]));

/*     Rectangle bin_rect = {
      r.x + (i * bin_w), 
      r.y + (r.height - bin_h), 
      bin_w, 
      bin_h
    };  */

    f32 hue = (f32)i / num_samples;
    Color c = ColorFromHSV(360 * hue, 1.0f, 1.0f);

    Vector2 start = {r.x + (i * bin_w), r.y + r.height};
    Vector2 end = {start.x, start.y - bin_h};
    push_line(start, end, thickness, c);
  }
}

INTERNAL f32
compute_correlation(f32 duration)
{
  f32 in_range = f32_map_to_range(0.f, duration, SQUARE(duration), 0.f, g_state->dataset_sum);
  return (in_range / g_state->dataset_sum);
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
  ClearBackground(COLOR_BG0);

  push_z_layer(Z_LAYER_NIL);
  push_alpha(1.0f);
  push_mouse_cursor(MOUSE_CURSOR_DEFAULT);

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
        strncpy(m->file_name, GetFileName(path), sizeof(m->file_name));

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
        g_state->num_loaded_music_files += 1;
      }
    }
    UnloadDroppedFiles(dropped_files);
  }

  // :update music
  MusicFile *active = DEREF_MUSIC_FILE_HANDLE(state->active_music_handle);
  if (IsKeyPressed(KEY_P))
  {
    if (IsMusicStreamPlaying(active->music)) 
    {
      PauseMusicStream(active->music);
      state->is_music_paused = true;
    }
    else 
    {
      ResumeMusicStream(active->music);
      state->is_music_paused = false;
    }
  }
  else if (IsKeyPressed(KEY_C))
  {
    StopMusicStream(active->music);
    PlayMusicStream(active->music);
  }
  UpdateMusicStream(active->music);

  if (!IsMusicReady(active->music))
  {
    const char *text = "Drag 'n' Drop Music";
    f32 font_size = rh * 0.15f;

    //Color backing_colour = {255, 173, 0, 255};
    Color backing_colour = COLOR_ORANGE_ACCENT;
    Color front_start_colour = WHITE;
    Color front_end_colour = {255, 222, 0, 255}; 

    f32 t_base = 0.4f;
    f32 t_range = 0.54f;

    f64 delta = (GetTime() - state->mouse_last_moved_time);

    f32 flash_duration = 1.0f; // 1 second
    f32 s = delta / flash_duration;
    s = SQUARE(s); // F32_SQRT(F32_ABS(s)); linear to hump
    s = 1 - s; // ease-out
    if (delta < flash_duration)
    {
      //Color new_front_start_colour = {255, 0, 255, 255};
      Color new_front_start_colour = COLOR_MAGENTA_ACCENT;
      front_start_colour = lerp_color(&front_start_colour, &new_front_start_colour, s);

      //Color new_front_end_colour = {255, 0, 0, 255};
      Color new_front_end_colour = COLOR_RED_ACCENT;
      front_end_colour = lerp_color(&front_end_colour, &new_front_end_colour, s);

      t_range += (0.2f - t_range) * s; 
    }

    f32 t = F32_COS(g_state->frame_counter * 3 * dt);
    t *= t;
    t = t_base + t_range * t;
    Color front_colour = lerp_color(&front_start_colour, &front_end_colour, t);

    u32 offset = state->font.baseSize / 40;

    Vector2 t_size = MeasureTextEx(state->font, text, font_size, 0.f);
    Vector2 t_vec = {
      rw/2.0f - t_size.x/2.0f,
      rh/2.0f - t_size.y/2.0f
    };
    Vector2 b_vec = {
      t_vec.x + offset,
      t_vec.y - offset,
    };

    push_text(text, state->font, font_size, t_vec, backing_colour);
    push_text(text, state->font, font_size, b_vec, front_colour);
  }
  else
  {
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
      if (power > max_power)
        max_power = power;
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
        if (p > bin_power)
          bin_power = p;
      }

      f32 target_t = bin_power / max_power;
      state->draw_samples[j] += (target_t - state->draw_samples[j]) * 8 * dt;

      j += 1;
    }

    Rectangle render_region = {0.f, 0.f, (f32)rw, (f32)rh};
    f32 color_region_t = 0.7f;
    Rectangle correlation_region = cut_rect_bottom(render_region, color_region_t);

    Rectangle top_region = cut_rect_top(render_region, color_region_t);
    f32 scroll_region_t = 0.2f;
    Rectangle scroll_region = cut_rect_left(top_region, scroll_region_t);
    Rectangle fft_region = cut_rect_right(top_region, scroll_region_t);

    draw_scroll_region(scroll_region);
    draw_fft(fft_region, state->draw_samples, num_bins);

    f32 correlation = compute_correlation(GetMusicTimeLength(active->music));
    draw_correlation_region(correlation_region, correlation);

    Rectangle text_r = {correlation_region.x + 50, correlation_region.y + 50, 600, 100};
    // draw_text_input(text_r);
  }

  render_elements();

  g_state->hover_consumed = false;
  g_state->left_click_consumed = false;

  g_dbg_at_y = 0.f;
  EndDrawing();
  }
}

PROFILER_END_OF_COMPILATION_UNIT
