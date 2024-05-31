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
  MusicFile *next, *prev;
  String8 name;
  Music music;
  u64 gen;
};

struct State
{
  MemArena *arena;
  MemArena *frame_arena;
  u64 frame_counter;

  // TODO(Ryan): Separate arena for music files
  MusicFile *first_mf, *last_mf;
  MusicFile *first_free_mf;
  Handle active_mf;

  SampleRing samples_ring;
  f32 hann_samples[NUM_SAMPLES];
  f32z fft_samples[NUM_SAMPLES];
  f32 draw_samples[HALF_SAMPLES];
};
GLOBAL State *g_state;




#endif
