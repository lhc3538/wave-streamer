#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "utils.h"
#include "wave_streamer.h"

/* globals */
static globals global;

/**
 * @brief help
 * Display a help message
 * @param progname
 * argv[0] is the program name and the parameter progname
 */
void help(char *progname)
{
    fprintf(stderr, "-----------------------------------------------------------------------\n");
    fprintf(stderr, "Usage: %s\n" \
            "  -i | --input \"<input-plugin.so> [parameters]\"\n" \
            "  -o | --output \"<output-plugin.so> [parameters]\"\n" \
            " [-h | --help ]........: display this help\n" \
            " [-v | --version ].....: display version information\n" \
            " [-b | --background]...: fork to the background, daemon mode\n", progname);
    fprintf(stderr, "-----------------------------------------------------------------------\n");
    fprintf(stderr, "Example #1:\n" \
            " To open an UVC webcam \"/dev/video1\" and stream it via HTTP:\n" \
            "  %s -i \"input_uvc.so -d /dev/video1\" -o \"output_http.so\"\n", progname);
    fprintf(stderr, "-----------------------------------------------------------------------\n");
    fprintf(stderr, "Example #2:\n" \
            " To open an UVC webcam and stream via HTTP port 8090:\n" \
            "  %s -i \"input_uvc.so\" -o \"output_http.so -p 8090\"\n", progname);
    fprintf(stderr, "-----------------------------------------------------------------------\n");
    fprintf(stderr, "Example #3:\n" \
            " To get help for a certain input plugin:\n" \
            "  %s -i \"input_uvc.so --help\"\n", progname);
    fprintf(stderr, "-----------------------------------------------------------------------\n");
    fprintf(stderr, "In case the modules (=plugins) can not be found:\n" \
            " * Set the default search path for the modules with:\n" \
            "   export LD_LIBRARY_PATH=/path/to/plugins,\n" \
            " * or put the plugins into the \"/lib/\" or \"/usr/lib\" folder,\n" \
            " * or instead of just providing the plugin file name, use a complete\n" \
            "   path and filename:\n" \
            "   %s -i \"/path/to/modules/input_uvc.so\"\n", progname);
    fprintf(stderr, "-----------------------------------------------------------------------\n");
}

/**
 * @brief signal_handler
 * pressing CTRL+C sends signals to this process instead of just
 * killing it plugins can tidily shutdown and free allocated
 * ressources. The function prototype is defined by the system,
 * because it is a callback function.
 * @param sig
 * sig tells us which signal was received
 */
void signal_handler(int sig)
{
    /* signal "stop" to threads */
    LOG("setting signal to stop\n");
    global.stop = 1;
    usleep(1000 * 1000);

    /* clean up threads */
    LOG("force cancellation of threads and cleanup resources\n");
    global.in.stop();
    global.out.stop();
    usleep(1000 * 1000);

    /* close handles of input plugins */
    dlclose(global.in.handle);
    dlclose(global.out.handle);
    DBG("all plugin handles closed\n");

    LOG("done\n");

    closelog();
    exit(0);
    return;
}

/**
 * @brief split_parameters
 * split the parameter by space to the argv[]
 * @param parameter_string
 * char string need to be split
 * @param argc
 * @param argv
 * @return
 */
int split_parameters(char *parameter_string, int *argc, char **argv)
{
    int count = 1;
    argv[0] = NULL; // the plugin may set it to 'INPUT_PLUGIN_NAME'
    if(parameter_string != NULL && strlen(parameter_string) != 0) {
        char *arg = NULL, *saveptr = NULL, *token = NULL;

        arg = strdup(parameter_string);

        if(strchr(arg, ' ') != NULL) {
            token = strtok_r(arg, " ", &saveptr);
            if(token != NULL) {
                argv[count] = strdup(token);
                count++;
                while((token = strtok_r(NULL, " ", &saveptr)) != NULL) {
                    argv[count] = strdup(token);
                    count++;
                    if(count >= MAX_PLUGIN_ARGUMENTS) {
                        IPRINT("ERROR: too many arguments to input plugin\n");
                        return 0;
                    }
                }
            }
        }
        free(arg);
    }
    *argc = count;
    return 1;
}

int main(int argc, char *argv[])
{
    //char *input  = "input_uvc.so --resolution 640x480 --fps 5 --device /dev/video0";
    char *input;
    char *output;
    int daemon = 0, i, j;
    size_t tmp = 0;

    output = "output_http.so --port 8080";

    /* parameter parsing */
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0
            },
            {"help", no_argument, 0, 0},
            {"i", required_argument, 0, 0},
            {"input", required_argument, 0, 0},
            {"o", required_argument, 0, 0},
            {"output", required_argument, 0, 0},
            {"v", no_argument, 0, 0},
            {"version", no_argument, 0, 0},
            {"b", no_argument, 0, 0},
            {"background", no_argument, 0, 0},
            {0, 0, 0, 0}
        };

        c = getopt_long_only(argc, argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help(argv[0]);
            return 0;
        }

        switch(option_index) {
            /* h, help */
        case 0:
        case 1:
            help(argv[0]);
            return 0;
            break;

            /* i, input */
        case 2:
        case 3:
            input = strdup(optarg);
            break;

            /* o, output */
        case 4:
        case 5:
            output = strdup(optarg);
            break;

            /* v, version */
        case 6:
        case 7:
            printf("MJPG Streamer Version: %s\n" \
            "Compilation Date.....: %s\n" \
            "Compilation Time.....: %s\n",
#ifdef SVN_REV
            SVN_REV,
#else
            SOURCE_VERSION,
#endif
            __DATE__, __TIME__);
            return 0;
            break;

            /* b, background */
        case 8:
        case 9:
            daemon = 1;
            break;

        default:
            help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    openlog("MJPG-streamer ", LOG_PID | LOG_CONS, LOG_USER);
    //openlog("MJPG-streamer ", LOG_PID|LOG_CONS|LOG_PERROR, LOG_USER);
    syslog(LOG_INFO, "starting application");

    /* fork to the background */
    if(daemon) {
        LOG("enabling daemon mode");
        daemon_mode();
    }

    /* ignore SIGPIPE (send by OS if transmitting to closed TCP sockets) */
    signal(SIGPIPE, SIG_IGN);

    /* register signal handler for <CTRL>+C in order to clean up */
    if(signal(SIGINT, signal_handler) == SIG_ERR) {
        LOG("could not register signal handler\n");
        closelog();
        exit(EXIT_FAILURE);
    }

    /*
     * messages like the following will only be visible on your terminal
     * if not running in daemon mode
     */
#ifdef SVN_REV
    LOG("MJPG Streamer Version: svn rev: %s\n", SVN_REV);
#else
    LOG("MJPG Streamer Version.: %s\n", SOURCE_VERSION);
#endif

    /* open input plugin */
    tmp = (size_t)(strchr(input, ' ') - input);
    global.in.stop      = 0;
    global.in.plugin = (tmp > 0) ? strndup(input, tmp) : strdup(input);
    global.in.handle = dlopen(global.in.plugin, RTLD_LAZY);
    if(!global.in.handle) {
        LOG("ERROR: could not find input plugin\n");
        LOG("       Perhaps you want to adjust the search path with:\n");
        LOG("       # export LD_LIBRARY_PATH=/path/to/plugin/folder\n");
        LOG("       dlopen: %s\n", dlerror());
        closelog();
        exit(EXIT_FAILURE);
    }
    global.in.init = dlsym(global.in.handle, "input_init");
    if(global.in.init == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    global.in.stop = dlsym(global.in.handle, "input_stop");
    if(global.in.stop == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    global.in.run = dlsym(global.in.handle, "input_run");
    if(global.in.run == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    global.in.add = dlsym(global.in.handle, "input_add");
    if(global.in.add == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    global.in.param.parameters = strchr(input, ' ');

    for (j = 0; j<MAX_PLUGIN_ARGUMENTS; j++) {
        global.in.param.argv[j] = NULL;
    }

    split_parameters(global.in.param.parameters, &global.in.param.argc, global.in.param.argv);
    global.in.param.global = &global;

    if(global.in.init(&global.in.param)) {
        LOG("input_init() return value signals to exit\n");
        closelog();
        exit(0);
    }

    /* open output plugin */
    tmp = (size_t)(strchr(output, ' ') - output);
    global.out.plugin = (tmp > 0) ? strndup(output, tmp) : strdup(output);
    global.out.handle = dlopen(global.out.plugin, RTLD_LAZY);
    if(!global.out.handle) {
        LOG("ERROR: could not find output plugin %s\n", global.out.plugin);
        LOG("       Perhaps you want to adjust the search path with:\n");
        LOG("       # export LD_LIBRARY_PATH=/path/to/plugin/folder\n");
        LOG("       dlopen: %s\n", dlerror());
        closelog();
        exit(EXIT_FAILURE);
    }
    global.out.init = dlsym(global.out.handle, "output_init");
    if(global.out.init == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    global.out.stop = dlsym(global.out.handle, "output_stop");
    if(global.out.stop == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    global.out.run = dlsym(global.out.handle, "output_run");
    if(global.out.run == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    global.out.add = dlsym(global.out.handle, "output_add");
    if(global.out.add == NULL) {
        LOG("%s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    for (j = 0; j<MAX_PLUGIN_ARGUMENTS; j++) {
        global.out.param.argv[j] = NULL;
    }
    split_parameters(global.out.param.parameters, &global.out.param.argc, global.out.param.argv);

    global.out.param.global = &global;
    if(global.out.init(&global.out.param)) {
        LOG("output_init() return value signals to exit\n");
        closelog();
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "starting input plugin %s", global.in.plugin);
    if(global.in.run()) {
        LOG("can not run input plugin %d: %s\n", i, global.in.plugin);
        closelog();
        return 1;
    }

    syslog(LOG_INFO, "starting output plugin: %s ", global.out.plugin);
    global.out.run();

    /* wait for signals */
    pause();

    return 0;
}



