// SPDX-License-Identifier: zlib-acknowledgement
#if !defined(APP_H)
#define APP_H

// IMPORTANT(Ryan): The number of samples limits number of frequencies we can derive
// This seems to be a good number for a decent visualisation
// Also, as displaying logarithmically, we don't actually have this large number
#define NUM_SAMPLES (1 << 13) 
STATIC_ASSERT(IS_POW2(NUM_SAMPLES));
#define HALF_SAMPLES (NUM_SAMPLES / 2)
struct SampleRing
{
  f32 samples[NUM_SAMPLES];
  u32 head;
};

typedef struct MusicFile MusicFile;
struct MusicFile
{
  MusicFile *next;
  Music music;
  u64 gen;
};
GLOBAL MusicFile g_zero_mf;

typedef struct MusicFileIndex MusicFileIndex;
struct MusicFileIndex
{
  MusicFileIndex *next;
  u32 index;
};

typedef struct FontNode FontNode;
struct FontNode {
  String8 key;
  FontNode *hash_next;
  u32 font;
};

typedef struct FontSlot FontSlot;
struct FontSlot {
  FontNode *first;
  FontNode *last;
};

// array of slots
INTERNAL void *
hash_find(void *slots, String8 key, memory_index slot_size, memory_index next_offset, memory_index value_offset)
{
  u64 hash = str8_hash(key);
  u64 slot_i = hash % 256;
  u8 *slot = (u8 *)slots + slot_i * slot_size; 

  u8 *first_node = slot;
  // IMPORTANT(Ryan): Assumes key is first element of node
  String8 node_key = *(String8 *)(first_node);
  if (str8_match(node_key, key, 0)) return (first_node + value_offset);

  for (u8 *chain_node = (first_node + next_offset);
       chain_node != NULL; 
       chain_node = (chain_node + next_offset))
  {
    node_key = *(String8 *)chain_node;
    if (str8_match(node_key, key, 0)) return (u8 *)(chain_node + value_offset);
  }

  return NULL;
}


// to write generic code in C, just focus on ptr and bytes (void* to u8*)
// same structure, different ending members
// require offsetof for padding

/*
typedef struct Assets Assets;
struct Assets
{
  FontSlot *font_slots;
  ImageSlot *image_slots;
  TextureSlot *texture_slots;
};
*/


// asset manager:
//   1. hotreloading requires global struct schema to stay same; so require list
//   2. also allows us to get from any location, e.g. filesystem, binary pack, etc.
//   3. will unload all assets on reload 

struct State
{
  MemArena *arena;
  MemArena *frame_arena;
  u64 frame_counter;

  f64 mouse_last_moved_counter;

#define MUSIC_FILE_POOL_SIZE 16
  MusicFile mf_pool[MUSIC_FILE_POOL_SIZE];
  MusicFileIndex mf_idx_pool[MUSIC_FILE_POOL_SIZE];
  MusicFileIndex *first_free_mf_idx;
  MusicFile *first_mf, *last_mf;
  Handle active_mf_handle;
  u32 num_mf;
  f32 mf_scroll_velocity;
  f32 mf_scroll;

  bool fullscreen_fft;
  Texture2D fullscreen_tex;

  Font font;

  SampleRing samples_ring;
  f32 hann_samples[NUM_SAMPLES];
  f32z fft_samples[NUM_SAMPLES];
  f32 draw_samples[HALF_SAMPLES];
};

typedef void (*app_preload_t)(State *s);
typedef void (*app_update_t)(State *s);
typedef void (*app_postload_t)(State *s);

typedef struct AppCode AppCode;
struct AppCode
{
  app_preload_t preload;
  app_update_t update;
  app_postload_t postload;
};



#endif
