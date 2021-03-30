// SPDX-License-Identifier: zlib-acknowledgement

#include <libudev.h>

#include <alsa/asoundlib.h>

#define ASSERT(expr) (expr) ? (void)0 : DEBUGGER_BREAK()
void 
DEBUGGER_BREAK(void) 
{ 
  char *errno_msg = strerror(errno);
  return; 
}

int main(int argc, char *argv[])
{
  // best to get information from ALSA, and just use udev to monitor if new sound device has been added
  void **hints = NULL;
  // pcm is an interface, what are the other interfaces?
  
  int card_num = -1;
  while (true) {
    if (snd_card_next(&card_num) < 0) {
      // TODO(Ryan): Logging 
      DEBUGGER_BREAK();
    }
    if (card_num == -1) {
      break; // no more cards
    }

    snd_ctl_t *card_handle = NULL;
    char str[64] = {0};
    sprintf(str, "hw:%i", card_num);
    // open cards control interface, not any device/sub-device
    if (snd_ctl_open(&card_handle, str, 0) < 0) {
      // TODO(Ryan): Logging 
      DEBUGGER_BREAK();
    }

    int dev_num = -1;
    while (true) {
      if (snd_ctl_pcm_next_device(card_handle, &dev_num) < 0) {
        // TODO(Ryan): Logging 
        DEBUGGER_BREAK();
      }
      // could have no waveform devices, e.g. MIDI card
      if (dev_num == -1) {
        break;
      }

      // to get information about sub devices
      snd_pcm_info_t *pcm_info = NULL;
      snd_pcm_info_alloca(&pcm_info);
      memset(pcm_info, 0, snd_pcm_info_sizeof());

      snd_pcm_info_set_device(pcm_info, dev_num);
      snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_PLAYBACK);


      // if just want to list, can use hw_params_test() 
      
      total_devices++;
    }
    snd_ctl_close(card_handle);
  } 

  // every card's wave device may have numerous sub-devices, e.g. input, output, surround sound, etc.
  
  // also have dmix, dsnoop? 
  // all information seems to be in /proc/asound so udev is of little help here
  
  // this won't return all alsa devices, e.g. bluetooth
  if (snd_device_name_hint(-1, "pcm", &hints) != 0) {
    // TODO(Ryan): Logging 
    DEBUGGER_BREAK();
  }

  for (int i = 0; hints[i] != NULL; ++i) {
    // can also get "DESC"
    char *name = snd_device_name_get_hint(hints[i], "NAME");

  }

  snd_device_name_free_hint(hints);

 
  return 0;
}
