#include "../wave_streamer.h"
#define OUTPUT_PLUGIN_PREFIX " o: "
#define OPRINT(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", OUTPUT_PLUGIN_PREFIX); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

/* parameters for output plugin */
typedef struct _output_parameter output_parameter;
struct _output_parameter {
    int id;
    char *parameters;
    int argc;
    char *argv[MAX_PLUGIN_ARGUMENTS];
    struct _globals *global;
};



/* structure to store variables/functions for output plugin */
typedef struct _output output;
struct _output {
    char *plugin;
    char *name;
    void *handle;

    output_parameter param;

    /* mutex for thread_id */
    pthread_mutex_t mutex;

    /* thread ids of data dealer*/
    pthread_t thread_id[MAX_USERS];
    int thread_count;

    int (*init)(input_parameter *);
    int (*stop)();
    int (*run)();
    int (*add)(int*);
};
