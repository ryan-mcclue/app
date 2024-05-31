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

  SampleRing samples_ring;
  f32 hann_samples[NUM_SAMPLES];
  f32z fft_samples[NUM_SAMPLES];
  f32 draw_samples[HALF_SAMPLES];
};
GLOBAL State *g_state;




#endif
