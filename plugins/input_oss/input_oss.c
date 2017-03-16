
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/soundcard.h>

#include "../../wave_streamer.h"
#include "../../utils.h"

#define INPUT_PLUGIN_NAME "OSS input plugin"
#define BUFFER_LENGTH 1024
/* private functions and variables to this plugin */
static globals     *pglobal;
/* oss fd */
int oss_fd;

int init_oss();
void *worker_in_thread( void *);
void *worker_out_thread( void *);
void worker_cleanup(void *);
void help(void);

//oss init parameter
char *dev = "/dev/dsp";
int bits = 16;
int rate = 16000;
int channels = 1;

/*** plugin interface functions ***/
int input_init(input_parameter *param) {
    int i;
    param->argv[0] = INPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c=0, format_temp = 16;
        static struct option long_options[] =
        {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"d", required_argument, 0, 0},
            {"device", required_argument, 0, 0},
            {"f", required_argument, 0, 0},
            {"format", required_argument, 0, 0},
            {"r", required_argument, 0, 0},
            {"rate", required_argument, 0, 0},
            {"c", required_argument, 0, 0},
            {"channels", required_argument, 0, 0},
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

            /* d, device */
        case 2:
        case 3:
            DBG("case 2,3\n");
            dev = strdup(optarg);
            break;

            /* f, format */
        case 4:
        case 5:
            DBG("case 4,5\n");
            bits = atoi(optarg);
            break;

            /* r, rate*/
        case 6:
        case 7:
            DBG("case 6,7\n");
            rate = atoi(optarg);
            break;

            /* c, channels*/
        case 8:
        case 9:
            DBG("case 8,9\n");
            channels = atoi(optarg);
            break;
        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    pglobal = param->global;

//    IPRINT("JPG input folder..: %s\n", folder);
//    IPRINT("delay.............: %i\n", delay);


    param->global->in.name = malloc((strlen(INPUT_PLUGIN_NAME) + 1) * sizeof(char));
    sprintf(param->global->in.name, INPUT_PLUGIN_NAME);

    return init_oss();
}

int input_stop() {
    DBG("will cancel input thread\n");
    return 0;
}

int input_run()
{
    return 0;
}

int input_add_out(int *pipe_fd)
{
    pthread_t worker_out;

    if( pthread_create(&worker_out, 0, worker_out_thread, (void *)pipe_fd) != 0) {
        fprintf(stderr, "could not start worker_out thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_detach(worker_out);
    return 0;
}

int input_add_in(int *pipe_fd)
{
    pthread_t worker_in;

    if( pthread_create(&worker_in, 0, worker_in_thread, (void *)pipe_fd) != 0) {
        fprintf(stderr, "could not start worker_in thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_detach(worker_in);
    return 0;
}

/*** private functions for this plugin below ***/
void help(void) {
    fprintf(stderr, " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
    " [-d | --delay ]........: delay to pause between frames\n" \
    " [-f | --folder ].......: folder to watch for new JPEG files\n" \
    " [-r | --remove ].......: remove/delete JPEG file after reading\n" \
    " [-n | --name ].........: ignore changes unless filename matches\n" \
    " [-e | --existing ].....: serve the existing *.jpg files from the specified directory\n" \
    " ---------------------------------------------------------------\n");
}

int init_oss()
{
    oss_fd = open(dev , O_RDWR);
    if(-1 == oss_fd )
    {
        perror("Open SoundCard Fail ... \n");
        return -1 ;
    }
    if(ioctl(oss_fd , SOUND_PCM_WRITE_RATE , &rate) < 0)
    {
        perror("write soundcard rate fail");
        return -1;
    }
    if(ioctl(oss_fd , SOUND_PCM_WRITE_CHANNELS, &channels) < 0)
    {
        perror("write soundcard channels fail");
        return -1;
    }
    if(ioctl(oss_fd , SOUND_PCM_WRITE_BITS ,&bits ) < 0)
    {
        perror("write soundcard bits fail");
        return -1;
    }
    IPRINT("rate:%d channels:%d bits:%d \n" ,
           rate , channels , bits);
    return 0;
}

/* the single stream thread */
void *worker_out_thread( void *arg ) {
    int *fd = (int*) arg;

    int err;
    char buffer[BUFFER_LENGTH];
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);

    while( !pglobal->stop ) {

        if (read(oss_fd,buffer,BUFFER_LENGTH) <= 0) {
            fprintf (stderr, "read from audio interface failed %d:(%s)\n",
                     err, snd_strerror (err));
            break;
        }

        /* copy frame from alsa to global buffer */
        if ((err = write(fd[1],buffer,sizeof(buffer)))<=0) {
            perror( "write output_fd filed");
            break;
        }
    }
    DBG("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);
    return NULL;
}

/* the  single stream  thread */
void *worker_in_thread( void *arg ) {
    int *fd = (int*) arg;

    int err;
    char buffer[BUFFER_LENGTH];
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);

    while( !pglobal->stop )
    {
        if ((err = read(fd[0],buffer,sizeof(buffer)))<=0) {
            perror( "read output_fd filed");
            break;
        }
        if (write(oss_fd,buffer,BUFFER_LENGTH) <= 0) {
            fprintf (stderr, "write from audio interface failed %d:(%s)\n",
                     err, snd_strerror (err));
            break;
        }
    }
    DBG("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);
    return NULL;
}

void worker_cleanup(void *arg) {
    static unsigned char first_run=1;

    if ( !first_run ) {
        DBG("already cleaned up ressources\n");
        return;
    }

    close(oss_fd);


    first_run = 0;
    DBG("audio interface closed\n");
}



