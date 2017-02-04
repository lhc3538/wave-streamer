#ifndef AUDIO_PIPE_H
#define AUDIO_PIPE_H
#define SOURCE_VERSION "1.0"

/* FIXME take a look to the output_http clients thread marked with fixme if you want to set more then 10 plugins */
#define MAX_PLUGIN_ARGUMENTS 32
#define MAX_USERS 100

#define DEBUG
#ifdef DEBUG
#define DBG(...) fprintf(stderr, " DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif

#define LOG(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

#include "plugins/input.h"
#include "plugins/output.h"

/* global variables that are accessed by all plugins */
typedef struct _globals globals;

struct _globals {
    int stop;

    /* input plugin */
    input in;

    /* output plugin */
    output out;

};

#endif


