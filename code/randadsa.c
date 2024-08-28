// SPDX-License-Identifier: zlib-acknowledgement
#if !TEST_BUILD
#define PROFILER 1
#endif

#include "desktop.h"

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
push_rect(Rectangle r, Color c)
{
  RenderElement *re = MEM_ARENA_PUSH_STRUCT_ZERO(g_state->frame_arena, RenderElement);
  re->type = RE_RECT;
  re->z = g_state->z_layer_stack->value;
  re->colour = {c.r, c.g, c.b, c.a * g_state->alpha_stack->value};

  re->rec = r;
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

  g_state->render_element_first = NULL;
  g_state->render_element_last = NULL;
  g_state->render_element_queue_count = 0;
  g_state->z_layer_stack = NULL;
  g_state->alpha_stack = NULL;
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
    push_rect(tooltip_rect, COLOR_BG0);
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
begin()
{
 //- rjf: prune all of the stale boxes
 for(U64 slot = 0; slot < ui_state->box_table_size; slot += 1)
 {
  for(UI_Box *box = ui_state->box_table[slot].first, *next = 0;
      !UI_BoxIsNil(box);
      box = next)
  {
   next = box->hash_next;
   if(UI_KeyMatch(box->key, UI_KeyZero()) || box->last_gen_touched+1 < ui_state->build_gen)
   {
    DLLRemove_NPZ(ui_state->box_table[slot].first, ui_state->box_table[slot].last, box, hash_next, hash_prev, UI_BoxIsNil, UI_BoxSetNil);
    StackPush(ui_state->first_free_box, box);
    ui_state->free_box_list_count += 1;
   }
  }
 }
}
   Box (active_t, hot_t, opacity, key, hashlinks)
    TODO: opacity is dealt globally like z index with push_opacity()
   hot if hover and pressed, active if just hover
   if(ev_key_is_mouse && ev_in_box_interaction_region && ev->kind == UI_EventKind_Press)
   {
    taken = 1;
    ui_state->hot_key = ui_state->active_key[ev_mb_slot] = box->key;
    sig.flags |= UI_SignalFlag_PressedLeft<<ev_mb_slot;
    ui_state->drag_start_mouse = ev->pos_2f32;
   }


root_function void
UI_AnimateRoot(UI_Box *root, F32 delta_time)
{
 //- rjf: calculate animation rates
 F32 slow_rate = 1 - Pow(2.f, -20.f * delta_time);
 F32 fast_rate = 1 - Pow(2.f, -50.f * delta_time);
 
 //- rjf: animate all boxes
 for(U64 slot = 0; slot < ui_state->box_table_size; slot += 1)
 {
  for(UI_Box *box = ui_state->box_table[slot].first; !UI_BoxIsNil(box); box = box->hash_next)
  {
   B32 is_hot          = UI_KeyMatch(ui_state->hot_key, box->key);
   B32 is_active       = UI_KeyMatch(ui_state->active_key[UI_MouseButtonSlot_Left], box->key);
   B32 is_disabled     = !!(box->flags & UI_BoxFlag_Disabled);
   B32 is_focus_hot    = !!(box->flags & UI_BoxFlag_FocusHot)    && !(box->flags & UI_BoxFlag_FocusHotDisabled);
   B32 is_focus_active = !!(box->flags & UI_BoxFlag_FocusActive) && !(box->flags & UI_BoxFlag_FocusActiveDisabled);
   box->hot_t              += ((F32)!!is_hot    - box->hot_t)                  * fast_rate;
   box->active_t           += ((F32)!!is_active - box->active_t)               * fast_rate;
   box->disabled_t         += ((F32)!!is_disabled - box->disabled_t)           * fast_rate;
   box->focus_hot_t        += ((F32)!!is_focus_hot - box->focus_hot_t)         * fast_rate;
   box->focus_active_t     += ((F32)!!is_focus_active - box->focus_active_t)   * fast_rate;
   box->view_off.x         += (box->target_view_off.x - box->view_off.x)       * fast_rate;
   box->view_off.y         += (box->target_view_off.y - box->view_off.y)       * fast_rate;
   if(AbsoluteValueF32(box->view_off.x - box->target_view_off.x) <= 1)
   {
    box->view_off.x = box->target_view_off.x;
   }
   if(AbsoluteValueF32(box->view_off.y - box->target_view_off.y) <= 1)
   {
    box->view_off.y = box->target_view_off.y;
   }
  }
 }
}
draw_boxes()





    for(U_WUINode *node = u_state->dev_wui_slots[idx].first; node != 0; node = node->hash_next)
    {
     B32 is_hot    = U_WUIKeyMatch(node->key, u_state->dev_wui_hot_key);
     B32 is_active = U_WUIKeyMatch(node->key, u_state->dev_wui_active_key[UI_MouseButtonSlot_Left]);
     node->hot_t    += rate * ((F32)!!is_hot - node->hot_t);
     node->active_t += rate * ((F32)!!is_active - node->active_t);
    }




INTERNAL COLOUR_PICKER_MODE
draw_mode()
{
  // char *hex_text = TextFormat("%02x%02x%02x%02x", value);
  // SetClipboardText(hex_text) if LEFT_DOWN over hex_text (if right str8_to_int(GetClipboardText()))
  // mode += 1 if LEFT_DOWN, -1 if RIGHT_DOWN over status
}

INTERNAL void
draw_color_circle(void)
{
  if (MODE_HSV)
  {
    Vector3 hsv = ColorToHSV(g_state->active_color);
    Color c = {hsv.x / 360.f, hsv.y, hsv.z, g_state->active_color.a};
    DrawCircleSector(cc, cc.x*.2f, 0, 360, 69, c);
  }

}


INTERNAL f32
draw_slider(Rectangle boundary, COLOR_MODE mode, f32 value, b32 *dragging, Colour c)
{
  Rectangle lr = boundary, sr = boundary, vr = boundary;
  char *value_text = TextFormat("%0.02f", mode_string(mode));
  char *s[COLOR_MODE_NUM][3] = { {"R", "G", "B"}, {"H", "S", "V"} };

  // ColorFromNormalized(state->red_slider.value, state->green)

  Rectangle r = align_rect(slider_dial, {8, 8}, RA_TOP);
  // counter clockwise
  DrawTriangle({r.x + r.width, r.y + r.height}, {}, {});

  DrawRectangleRounded(x, x + v);
  DrawRectangleRounded(x + v, w);

  Color color_left = c;
  Color color_right = c;
  color_left.e[color_component] = 0;
  color_right.e[color_component] = 255;
  DrawRectangleGradientEx(left, color_left, color_left, color, color);
  DrawRectangleGradientEx(right, color, color, color_right, color_right);

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
draw_scroll_region(Rectangle r)
{
  push_rect(r, COLOR_BG1);

  f32 music_file_size_h = r.width * 0.2f;
  f32 padding = music_file_size_h * 0.1f;
  f32 music_file_w = r.width - padding*2.f;
  f32 music_file_h = music_file_size_h - padding*2.f;

  f32 at_y = r.y + 10.f;
  for (u32 i = 0; i < ARRAY_COUNT(g_state->music_files); i += 1)
  {
    MusicFile *m = &g_state->music_files[i];
    if (!m->is_active) continue;

    Rectangle music_file_r = {r.x + padding, 
                              r.y + i*music_file_size_h + padding,
                              music_file_w,
                              music_file_h};
    push_rect(music_file_r, COLOR_RED_ACCENT);
    push_text(m->file_name, g_state->font, 30.f, {music_file_r.x, music_file_r.y}, WHITE);
  }

  //f32 font_size = g_state->font.baseSize * (r.width / r.height);

/*   Rectangle label_r = {r.x, r.y, r.width, 50};
  Color bg_color = ColorBrightness(COLOR_YELLOW_ACCENT, 0.5);
  if (consume_hover(label_r))
  {
    bg_color = ColorBrightness(COLOR_YELLOW_ACCENT, 0.7);
  }
  push_rect(label_r, bg_color);
  push_rect({r.x, r.y+20, 20, 20}, COLOR_BLUE_ACCENT);
  push_text("hi there", g_state->font, 40.f, {r.x + 30.f, r.y+20}, COLOR_FONT); */
}
// IMPORTANT: GetCollisionRec(panel, item)
// this computes the intersection of two rectangles
// useful for BeginScissorMode()

INTERNAL void
draw_color_picker(Rectangle r)
{
  push_rect(r, COLOR_BG1);
}

INTERNAL void
draw_fft(Rectangle r, f32 *samples, u32 num_samples)
{
  DrawRectangleRec(r, COLOR_BG0);

  MusicFile *active = DEREF_MUSIC_FILE_HANDLE(g_state->active_music_handle);
  if (!ZERO_MUSIC_FILE(active))
  {
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

  draw_scroll_region(scroll_region);
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

#define MAX_MUSIC_FILE_NAME_LENGTH 64
typedef struct MusicFile MusicFile;
struct MusicFile
{
  char file_name[MAX_MUSIC_FILE_NAME_LENGTH];
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
  Z_LAYER_FFT_BG,
  Z_LAYER_FFT_FG,
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

typedef struct AlphaNode AlphaNode; 
struct AlphaNode
{
  AlphaNode *next;
  f32 value;
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

  AlphaNode *alpha_stack;
  ZLayerNode *z_layer_stack;
  RenderElement *render_element_first, *render_element_last;
  u32 render_element_queue_count;

  MusicFile music_files[MAX_MUSIC_FILES];
  Handle active_music_handle;
  b32 is_music_paused;

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
