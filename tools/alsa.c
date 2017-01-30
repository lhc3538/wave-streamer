#include "alsa.h"
int init_alsa(snd_pcm_t **handle, char* drivice,unsigned int rate,snd_pcm_format_t format,int channels)
{
    snd_pcm_hw_params_t *hw_params;
    if ((err = snd_pcm_open (handle, drivice, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%s)\n",
                 argv[1],
                snd_strerror (err));
        return 1;
    }

    DBG( "audio interface opened");

    if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "hw_params allocated");

    if ((err = snd_pcm_hw_params_any (*handle, hw_params)) < 0) {
        fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "hw_params initialized");

    if ((err = snd_pcm_hw_params_set_access (*handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf (stderr, "cannot set access type (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "hw_params access setted");

    if ((err = snd_pcm_hw_params_set_format (*handle, hw_params, format)) < 0) {
        fprintf (stderr, "cannot set sample format (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "hw_params format setted");

    if ((err = snd_pcm_hw_params_set_rate_near (*handle, hw_params, &rate, 0)) < 0) {
        fprintf (stderr, "cannot set sample rate (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "hw_params rate setted");

    if ((err = snd_pcm_hw_params_set_channels (*handle, hw_params, channels)) < 0) {
        fprintf (stderr, "cannot set channel count (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "hw_params channels setted");

    if ((err = snd_pcm_hw_params (*handle, hw_params)) < 0) {
        fprintf (stderr, "cannot set parameters (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "hw_params setted");

    snd_pcm_hw_params_free (hw_params);

    DBG( "hw_params freed");

    if ((err = snd_pcm_prepare (*handle)) < 0) {
        fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
                 snd_strerror (err));
        return 1;
    }

    DBG( "audio interface prepared");

}
