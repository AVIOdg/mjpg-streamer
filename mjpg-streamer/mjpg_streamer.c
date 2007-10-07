/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "utils.h"
#include "mjpg_streamer.h"

/* globals */
globals global;

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
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

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void signal_handler(int sig)
{
  int i;

  /* signal "stop" to threads */
  fprintf(stderr, "setting signal to stop\n");
  global.stop = 1;
  usleep(1000*1000);

  /* clean up threads */
  fprintf(stderr, "force cancelation of threads and cleanup ressources\n");
  global.in.stop();
  for(i=0; i<global.outcnt; i++)
    global.out[i].stop();

  usleep(1000*1000);

  dlclose(&global.in.handle);
  for(i=0; i<global.outcnt; i++)
    dlclose(&global.out[i].handle);

  pthread_cond_destroy(&global.db_update);
  pthread_mutex_destroy(&global.db);

  fprintf(stderr, "done\n");

  exit(0);
  return;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int main(int argc, char *argv[])
{
  char *input  = "input_uvc.so --resolution 640x480 --fps 5 --device /dev/video0";
  char *output[MAX_OUTPUT_PLUGINS];
  int daemon=0, i;
  size_t tmp=0;

  output[0] = "output_http.so --port 8080";
  global.outcnt = 0;

  /* parameter parsing */
  while(1) {
    int option_index = 0, c=0;
    static struct option long_options[] = \
    {
      {"h", no_argument, 0, 0},
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
    if (c == -1) break;

    /* unrecognized option */
    if(c=='?'){ help(argv[0]); return 0; }

    switch (option_index) {
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
        output[global.outcnt++] = strdup(optarg);
        break;

      /* v, version */
      case 6:
      case 7:
        printf("MJPG Streamer Version: %s\n" \
               "Compilation Date.....: %s\n" \
               "Compilation Time.....: %s\n", SOURCE_VERSION, __DATE__, __TIME__);
        return 0;
        break;

      /* v, version */
      case 8:
      case 9:
        daemon=1;
        break;

      default:
        help(argv[0]);
        return 0;
    }
  }

  /* fork to the background */
  if ( daemon ) {
    daemon_mode();
  }

  /* initialise the global variables */
  global.stop      = 0;
  global.buf       = NULL;
  global.size      = 0;
  global.in.plugin = NULL;

  if( pthread_mutex_init(&global.db, NULL) != 0 ) {
    fprintf(stderr, "could not initialize mutex variable\n");
    exit(EXIT_FAILURE);
  }
  if( pthread_cond_init(&global.db_update, NULL) != 0 ) {
    fprintf(stderr, "could not initialize condition variable\n");
    exit(EXIT_FAILURE);
  }

  /* ignore SIGPIPE (send by OS if transmitting to closed TCP sockets) */
  signal(SIGPIPE, SIG_IGN);

  /* register signal handler for <CTRL>+C in order to clean up */
  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    fprintf(stderr, "could not register signal handler\n");
    exit(EXIT_FAILURE);
  }

  /* messages like the following will only be visible if not running in daemon mode */
  fprintf(stderr, "MJPG Streamer Version.: %s\n", SOURCE_VERSION);

  /* check if at least one output plugin was selected */
  if ( global.outcnt == 0 ) {
    /* no? Then use the default plugin instead */
    global.outcnt = 1;
  }

  /* open input plugin */
  tmp = (size_t)(strchr(input, ' ')-input);
  global.in.plugin = (tmp > 0)?strndup(input, tmp):strdup(input);
  global.in.handle = dlopen(global.in.plugin, RTLD_LAZY);
  if ( !global.in.handle ) {
    fprintf(stderr, "ERROR: could not find input plugin\n");
    fprintf(stderr, "       Perhaps you want to adjust the search path with:\n");
    fprintf(stderr, "       # export LD_LIBRARY_PATH=/path/to/plugin/folder\n");
    fprintf(stderr, "dlopen: %s\n", dlerror() );
    exit(EXIT_FAILURE);
  }
  global.in.init = dlsym(global.in.handle, "input_init");
  if ( global.in.init == NULL ) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  global.in.stop = dlsym(global.in.handle, "input_stop");
  if ( global.in.stop == NULL ) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  global.in.run = dlsym(global.in.handle, "input_run");
  if ( global.in.run == NULL ) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  /* try to find optional command */
  global.in.cmd = dlsym(global.in.handle, "input_cmd");

  global.in.param.parameter_string = strchr(input, ' ');
  global.in.param.global = &global;
  if ( global.in.init(&global.in.param) ) {
    DBG("input_init() return value signals to exit\n");
    exit(0);
  }

  /* open output plugin */
  for (i=0; i<global.outcnt; i++) {
    tmp = (size_t)(strchr(output[i], ' ')-output[i]);
    global.out[i].plugin = (tmp > 0)?strndup(output[i], tmp):strdup(output[i]);
    global.out[i].handle = dlopen(global.out[i].plugin, RTLD_LAZY);
    if ( !global.out[i].handle ) {
      fprintf(stderr, "ERROR: could not find output plugin\n");
      fprintf(stderr, "       Perhaps you want to adjust the search path with:\n");
      fprintf(stderr, "       # export LD_LIBRARY_PATH=/path/to/plugin/folder\n");
      fprintf(stderr, "dlopen: %s\n", dlerror() );
      exit(EXIT_FAILURE);
    }
    global.out[i].init = dlsym(global.out[i].handle, "output_init");
    if ( global.out[i].init == NULL ) {
      fprintf(stderr, "%s\n", dlerror());
      exit(EXIT_FAILURE);
    }
    global.out[i].stop = dlsym(global.out[i].handle, "output_stop");
    if ( global.out[i].stop == NULL ) {
      fprintf(stderr, "%s\n", dlerror());
      exit(EXIT_FAILURE);
    }
    global.out[i].run = dlsym(global.out[i].handle, "output_run");
    if ( global.out[i].run == NULL ) {
      fprintf(stderr, "%s\n", dlerror());
      exit(EXIT_FAILURE);
    }
    /* try to find optional command */
    global.out[i].cmd = dlsym(global.out[i].handle, "output_cmd");

    global.out[i].param.parameter_string = strchr(output[i], ' ');
    global.out[i].param.global = &global;
    if ( global.out[i].init(&global.out[i].param) ) {
      DBG("output_init() return value signals to exit\n");
      exit(0);
    }
  }

  /* start to read the camera, push picture buffers into global buffer */
  DBG("starting input plugin\n");
  global.in.run();

  DBG("starting %d output plugin(s)\n", global.outcnt);
  for(i=0; i<global.outcnt; i++)
    global.out[i].run();

  /* wait for signals */
  pause();

  return 0;
}