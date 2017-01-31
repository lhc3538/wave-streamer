#pragma once
#include <alsa/asoundlib.h>
int init_alsa(snd_pcm_t **handle,char* drivice,unsigned int rate,snd_pcm_format_t format,int channels);
