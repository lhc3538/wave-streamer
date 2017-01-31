#include <syslog.h>
#include "../wave_streamer.h"
#define INPUT_PLUGIN_PREFIX " i: "
#define IPRINT(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", INPUT_PLUGIN_PREFIX); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

/* parameters for input plugin */
typedef struct _input_parameter input_parameter;
struct _input_parameter {
    int id;
    char *parameters;
    int argc;
    char *argv[MAX_PLUGIN_ARGUMENTS];
    struct _globals *global;
};

/* structure to store variables/functions for input plugin */
typedef struct _input input;
struct _input {
    char *plugin;
    char *name;
    void *handle;

    input_parameter param; // this holds the command line arguments

    /* mutex for thread_id */
    pthread_mutex_t db;

    /* thread ids of data dealer*/
    pthread_t thread_id[MAX_USERS];
    int thread_count;

    int (*init)(input_parameter *);
    int (*stop)();
    int (*run)();
    int (*add)(int);
};
