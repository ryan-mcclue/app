#if !defined(APP_H)
#define APP_H

#include "base/base-inc.h"
#include "app-assets.h"
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
  RA_CENTRE,
  RA_LEFT_INNER
} RECT_ALIGN;

typedef enum
{
  RE_NIL = 0,
  RE_RECT,
  RE_RECT_OUTLINE,
  RE_TEXT,
  RE_CIRCLE,
  RE_LINE,
  RE_TEXTURE,
} RENDER_ELEMENT_TYPE;

typedef enum
{
  Z_LAYER_NIL = 0,
  Z_LAYER_TOOLTIP,
} Z_LAYER;

typedef struct RenderElement RenderElement;
struct RenderElement
{
  RENDER_ELEMENT_TYPE type;

  RenderElement *next;

  Z_LAYER z;
  Color colour;

  Rectangle rec;
  f32 roundness;
  u32 segments;

  f32 radius;

  f32 thickness;

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

typedef struct MouseCursorNode MouseCursorNode; 
struct MouseCursorNode
{
  MouseCursorNode *next;
  MouseCursor value;
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

  MouseCursorNode *mouse_cursor_stack;
  AlphaNode *alpha_stack;
  ZLayerNode *z_layer_stack;
  RenderElement *render_element_first, *render_element_last;
  u32 render_element_queue_count;
  u64 active_button_id;

  b32 text_input_active;
  char text_input_buffer[64];
  u32 text_input_buffer_at;
  u32 text_input_buffer_len;
  RangeU32 text_input_selection;

  u32 text_input_val;

  MusicFile music_files[MAX_MUSIC_FILES];
  Handle active_music_handle;
  u32 num_loaded_music_files;
  f32 active_music_slider_value;
  b32 active_music_slider_dragging;
  b32 is_music_paused;

  f32 scroll;
  f32 scroll_velocity;

  SampleRing samples_ring;
  f32 hann_samples[NUM_SAMPLES];
  f32z fft_samples[NUM_SAMPLES];
  f32 draw_samples[HALF_SAMPLES];

  f32 mouse_last_moved_time;

  u32 dataset_sum;
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
