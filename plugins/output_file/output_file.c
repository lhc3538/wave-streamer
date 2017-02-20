#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#include "../../utils.h"
#include "../../wave_streamer.h"
#include "../../tools/alsa.h"

#define OUTPUT_PLUGIN_NAME "file output plugin"

static pthread_t worker;
static globals *pglobal;

typedef struct
{
    char chRIFF[4];                 // "RIFF" 标志
    int  total_Len;                 // 文件长度
    char chWAVE[4];                 // "WAVE" 标志
    char chFMT[4];                  // "fmt" 标志
    int  dwFMTLen;                  // 过渡字节（不定）  一般为16
    short fmt_pcm;                  // 格式类别
    short  channels;                // 声道数
    int fmt_samplehz;               // 采样率
    int fmt_bytepsec;               // 位速
    short fmt_bytesample;           // 一个采样多声道数据块大小
    short fmt_bitpsample;
    // 一个采样占的 bit 数
    char chDATA[4];                 // 数据标记符＂data ＂
    int  dwDATALen;                 // 语音数据的长度，比文件长度小42一般。这个是计算音频播放时长的关键参数~
}wave_header;

/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this plugin:\n\n" \
            " [-f | --folder ]........: folder to save pictures\n" \
            " [-m | --mjpeg ].........: save the frames to an mjpg file \n" \
            " [-d | --delay ].........: delay after saving pictures in ms\n" \
            " [-i | --input ].........: read frames from the specified input plugin\n" \
            " The following arguments are takes effect only if the current mode is not MJPG\n" \
            " [-s | --size ]..........: size of ring buffer (max number of pictures to hold)\n" \
            " [-e | --exceed ]........: allow ringbuffer to exceed limit by this amount\n" \
            " [-c | --command ].......: execute command after saving picture\n"\
            " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: clean up allocated ressources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg) {
    static unsigned char first_run=1;

    if ( !first_run ) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;

    OPRINT("cleaning up ressources allocated by worker thread\n");

}

/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame and stores it to file
Input Value.:
Return Value:
******************************************************************************/
void *worker_thread( void *arg )
{
    //alsa init parameter
    char *dev = "default";
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    unsigned int rate = 16000;
    unsigned int channels = 1;

    int err;
    int buffer_frames = 512;
    int buffer_length = buffer_frames * snd_pcm_format_width(format)/8 * channels;
    char buffer[buffer_length];
    memset(buffer,-1,buffer_length);

    snd_pcm_t *capture_handle;
    init_alsa(&capture_handle,dev,rate,format,channels);

    int file_fd,size;
    file_fd = open("/tmp/test.wav",O_WRONLY|O_CREAT);

    wave_header wav_head_data = {
        "RIFF",
        0 ,
        "WAVE",
        "fmt ",
        16,
        1,
        1,
        16000,
        16000*16,
        32,
        16,
        "data",
        0
    };
    size = write(file_fd,(char*)&wav_head_data,sizeof(wav_head_data));
    if (size <= 0)
    {
        fprintf(stderr,"write file err\n");
    }

    int test_count = 0;
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);
    while(!pglobal->stop)
    {
        if (test_count != 0)
            --test_count;
        else
        {
            if ((err = snd_pcm_readi (capture_handle, buffer, buffer_frames)) != buffer_frames) {
                fprintf (stderr, "read from audio interface failed %d:(%s)\n",
                         err, snd_strerror (err));
                break;
            }
        }
        size = write(file_fd,buffer,buffer_length);
        if (size <= 0)
        {
            fprintf(stderr,"write file err\n");
            break;
        }
    }
    close(file_fd);
    snd_pcm_close(capture_handle);
    pthread_cleanup_pop(1);
    return NULL;
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: this function is called first, in order to initialise
              this plugin and pass a parameter string
Input Value.: parameters
Return Value: 0 if everything is ok, non-zero otherwise
******************************************************************************/
int output_init(output_parameter *param)
{
    int i;
    pglobal = param->global;
    pglobal->out.name = malloc((1+strlen(OUTPUT_PLUGIN_NAME))*sizeof(char));
    sprintf(pglobal->out.name, "%s", OUTPUT_PLUGIN_NAME);

    DBG("OUT plugin name: %s\n", pglobal->out.name);

    param->argv[0] = OUTPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c=0;
        static struct option long_options[] = \
        {
        {"h", no_argument, 0, 0},
        {"help", no_argument, 0, 0},
        {"p", required_argument, 0, 0},
        {"port", required_argument, 0, 0},
        {"q", required_argument, 0, 0},
        {"queue", required_argument, 0, 0},
        {0, 0, 0, 0}
    };

        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if (c == -1) break;

        /* unrecognized option */
        if (c == '?'){
            help();
            return 1;
        }

        switch (option_index) {
        /* h, help */
        case 0:
        case 1:
            DBG("case 0,1\n");
            help();
            return 1;
            break;

        /* p, port */
        case 2:
        case 3:
            DBG("case 2,3\n");
//            port = atoi(optarg);
            break;

        /* q, queue */
        case 4:
        case 5:
//            queue = atoi(optarg);
            DBG("case 4,5\n");
            break;

        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    OPRINT("input plugin.....: %s\n",  pglobal->in.plugin);

    return 0;
}

/******************************************************************************
Description.: calling this function stops the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_stop() {
    DBG("will cancel worker thread\n");
    pthread_cancel(worker);
    return 0;
}

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_run() {
    DBG("launching worker thread\n");
    pthread_create(&worker, 0, worker_thread, NULL);
    pthread_detach(worker);
    return 0;
}

int output_add() {
    return 0;
}
