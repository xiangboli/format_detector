/*!
 *  \file     format_detector.h
 *  \brief    Format pattern analysis application
 *
 *  Internal structures, definitions, and prototypes for Format Detector tool. 
 * 
 *  \version  1.0.00
 *  \date     Tue Feb. 5, 2019
 *
 *  \authors  Yuriy Reznik
 *            Xiangbo Li
 *
 *  \copyright (c) 2019 Brightcove, Inc.
 */


/* system-dependent stuff: */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#include <direct.h>
#define DIRSEP '\\'
#define _CRT_SECURE_NO_WARNINGS
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <stdint.h>
#define _unlink unlink
#define _rmdir rmdir
#define DIRSEP '/'
#include <strings.h> // for strcasecmps
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "getopt.h"
#include "format_detector.h"
#include "timer.h"

/*! Extract integer */
static int get_int (char *s, int *x, int x_min, int x_max)
{
  /* sanity checks */
  assert(s != NULL);
  assert(x != NULL);
  assert(x_min <= x_max);

  /* read integer */
  *x = (int)strtol(s, &s, 10);

  /* check range */
  return (*x < x_min || *x > x_max)? 1: 0;
}

/*! Extract framerate */
static int get_framerate (char *s, fps_t *fps)
{
  /* sanity checks */
  assert (s != NULL);
  assert (fps != NULL);

  /* assign 0s */
  fps->num = fps->denom = 0;

  /* read framerate */
  if (strchr(s, '/') || strchr(s, ':')) {     // fraction
    fps->num = (int)strtol(s, &s, 10);
    if (strlen(s) > 1) fps->denom = (int)strtol(s + 1, &s, 10);
  } else if (strchr(s, '.')) {                // float
    fps->num = (int)(strtof(s, &s) * 1000);
    fps->denom = 1000;
  } else {                                    // integer
    fps->num = (int)strtol(s, &s, 10);
    fps->denom = 1;
  }

  /* check if framerate is valid */
  return (fps->num <= 0 || fps->denom <= 0 || fps_to_float(*fps) < 0.1f || fps_to_float(*fps) > 300.0)? 1: 0;
}

/*! Extract resolution */
static int get_resolution (char *s, res_t *res)
{
  /* sanity checks */
  assert(s != NULL);
  assert(res != NULL);

  /* assign 0s */
  res->width = res->height = 0;

  /* read resolution */
  res->width = (int)strtol(s, &s, 10);
  printf("Width: %d   ", res->width);
  if (strlen(s) > 1) res->height = (int)strtol(s + 1, &s, 10);
  printf("Height: %d", res->height);

  /* check if resolution is valid */
  return (res->width < 0 || res->height < 0 || res->width > MAX_WIDTH || res->height > MAX_HEIGHT || res->height & 1)? 1: 0;
}

/*! Chroma subsamping format & bitdepth parameters */
static int get_format(char *arg, int *format, int *bitdepth)
{
  /* sanity checks */
  assert(arg != NULL);
  assert(format != NULL);
  assert(bitdepth != NULL);

  /* assign default value: */
  *format = FORMAT_UNKNOWN;
  *bitdepth = 8;

  /* check string values: */
  if (!strcasecmp(arg, "yuv420p") || !strcasecmp(arg, "i420") || !strcasecmp(arg, "iyuv") || !strcasecmp(arg, "yv12") ||  !strcasecmp(arg, "nv12")) {*format = FORMAT_YUV420; *bitdepth = 8;}
  else if (!strcasecmp(arg, "yuv422p") || !strcasecmp(arg, "i422")) {*format = FORMAT_YUV422; *bitdepth = 8;}
  else if (!strcasecmp(arg, "yuv444p") || !strcasecmp(arg, "i444")) {*format = FORMAT_YUV444; *bitdepth = 8;}
  else if (!strcasecmp(arg, "yuv420p10le")) {*format = FORMAT_YUV420; *bitdepth = 10;}
  else if (!strcasecmp(arg, "yuv422p10le")) {*format = FORMAT_YUV422; *bitdepth = 10;}
  else if (!strcasecmp(arg, "yuv444p10le")) {*format = FORMAT_YUV444; *bitdepth = 10;}
  else return 1;

  /* success: */
  return 0;
}

/******************************************************* 
 * 
 *  Error, notification, & commandline reading functions: 
 *
 *  error()
 *  version()
 *  help()
 *  read_command_line()
 * 
 ****/

/*! Print error message & possibly exit */
void error (int terminate, const char * format, ...)
{
  va_list args;
  va_start (args, format);
  fprintf (stderr,"ERROR: ");
  vfprintf (stderr, format, args);
  va_end (args);
  if (terminate)
    exit(1);
}

/*! Print program name & version */
static void version ()
{
  printf ("Brightcove Telecine Detector. Version %s (%s)\n", VERSION, __DATE__);
  printf ("Copyright (c) 2019 Brightcove, Inc. All Rights Reserved.\n\n");
}

/*! Print help screen */
static void help (char *prog)
{
  char *s; if ((s = strrchr(prog, '\\')) != 0) prog = s + 1;

  /* print help screen */
  printf (
    "Usage: %s [-i] input [-options] \n"
    "\n"
    "Options:\n"
    "\n"
    "  -i, --input       <string>             Name of uncompressed video file to be analyzed (.yuv or .y4m)\n"
    "  -r, --resolution  <int x int>          Video resolution (width x height, in pixels)\n"
    "  -f, --framerate   <float or fraction>  Framerate (in fps)\n"
    "  -c, --csp         <string>             Chroma sub-sampling format (e.g. \"yuv420p\", \"yuv422p\", etc)\n"
    "  -y  --temp_dir <directory>             Directory to use for intermediate files\n"
    "  -v, --verbose                          Print internal statistics & debug information\n"
    "  -h, --help                             Display help\n"
    "\n",
   prog);
  exit(1);
}

/*! Read program command-line  */
static void read_command_line(int argc, char *argv[], char **input, res_t *resolution, fps_t *framerate, int *format, char **temp_dir, int *bitdepth, int *verbose)
{
  /* command-line parsing structure */
  static char optstring[] = "i:r:f:c:y:vh";
  static struct option long_options[] = 
  {
    {"input",       required_argument, 0, 'i'},
    {"resolution",  required_argument, 0, 'r'},
    {"framerate",   required_argument, 0, 'f'},
    {"csp",         required_argument, 0, 'c'},
    {"temp_dir",    required_argument, 0, 'y'},
    {"verbose",     no_argument,       0, 'v'},
    {"help",        no_argument,       0, 'h'},
    {0,             0,                 0, 0}
  };
  int long_index = 0, i;

  /* parse command line */
  while ((i = getopt_long(argc, argv, optstring, long_options, &long_index)) != -1) 
  {
    /* skip initial assignment character */
    if (optarg != NULL && (optarg[0] == '=' || optarg[0] == ':')) optarg++;   
    /* process option */
    switch (i) 
    {
      case 'i': if ((*input = optarg) == NULL)                            goto valerr; break;
      case 'r': if (get_resolution (optarg, resolution))                  goto valerr; break;
      case 'f': if (get_framerate (optarg, framerate))                    goto valerr; break;
      case 'c': if (get_format (optarg, format, bitdepth))                goto valerr; break;
      case 'y': if ((*temp_dir = optarg) == NULL)                         goto valerr; break;
      case 'v': *verbose = 1;                                             break;
      case 'h': default: help(argv[0]);
       /* errors */
      valerr:
        error (1, "Invalid parameter value: %s = %s\n", argv[optind-1], optarg);

    }
  }

  /* check if there are non-optional arguments */
  if (argc - optind >= 2)
    help(argv[0]);
  if (argc - optind == 1) {
    if (*input != NULL) help(argv[0]);
    *input = argv[optind + 0];
  }

  /* check if input file is specified */
  if (*input == NULL) error (1, "Input video file is not specified.\n");

  /* check presence of mandatory parameters: */
  if (!resolution->height || !resolution->width) error (1, "Video resolution must be specified.\n");
  if (!framerate->num || !framerate->denom) error (1, "Video framerate must be specified.\n");
}

/*! 
 *  \brief Compute size of an image/frame stored using a given format 
 */
static int frame_size (res_t *res, int format, int bitdepth)
{
  int size = 0;

  /* sanity checks */
  assert(res != NULL);

  /* compute image size based on format: */
  switch (format)  {
    case FORMAT_YUV420:   size = res->height * res->width * 3/2;  break;
    case FORMAT_YUV422:   size = res->height * res->width * 2;    break;
    case FORMAT_YUV444:   size = res->height * res->width * 3;    break;
  }

  /* adjust based on pixel depth: */
  if (bitdepth > 8)
    size *= 2; 

  /* return 0 if error, frame size otherwize */
  return size;
}

/*!
 * @brief Given a frame, calculate the average pixel difference between odd fields (delta_odd) and even fields (delta_even)
 * 
 * @param[in] frame 
 * @param[in] res 
 * @param[out] delta_even  
 * @param[out] delta_odd 
 */
void calculate_field_delta(unsigned char *frame, res_t *res, float *delta_even, float *delta_odd)
{
  int i, j, d_even =0, d_odd=0, dd_even =0, dd_odd=0;
  timestamp_t start_time, stop_time;   /* runtimes for each pass */
  double exec_time_c=0.0, exec_time_avx=0.0;
  int pitch = 16;
  int n = res->width/pitch;
  int dd_even_avx=0, dd_odd_avx=0;

  get_time(&start_time);
  for (i=0; i<(res->height/2-1); i++){
    for(j=0; j<res->width; j++) {
        d_even = frame[2*i*res->width + j]-frame[2*(i+1)*res->width + j];
        dd_even += d_even * d_even;

        d_odd = frame[(2*i+1)*res->width + j]-frame[(2*i+3)*res->width + j];   
        dd_odd += d_odd * d_odd;  
      }                   
  }
  get_time (&stop_time);
  exec_time_c = elapsed_time (&start_time, &stop_time);
  printf("\n");
  printf("dd_even_norm: %-16d      dd_odd_norm:%-13d     norm_t: %f\n", dd_even, dd_odd, exec_time_c);

  get_time(&start_time);
  for (i=0; i<(res->height/2-1); i++){
    dd_even_avx += ssd_nx16_u8_avx2_intrin (&frame[2*i*res->width], &frame[2*(i+1)*res->width], pitch, n);
    dd_odd_avx += ssd_nx16_u8_avx2_intrin (&frame[(2*i+1)*res->width], &frame[(2*i+3)*res->width], pitch, n);
  }
  get_time (&stop_time);
  exec_time_avx = elapsed_time (&start_time, &stop_time);
  printf("dd_even_avx2: %-16d      dd_odd_avx2:%-16d  avx2_t: %f\n", dd_even_avx, dd_odd_avx, exec_time_avx);

  *delta_even = (float)dd_even / ((res->height/2 -1)*res->width);
  *delta_odd = (float)dd_odd / ((res->height/2 -1)*res->width);
}

/*!
 * @brief Given a frame, calculate the average vertical pixel value change in frame (delta)
 * 
 * @param[in] frame 
 * @param[in] res 
 * @param[out] delta 
 */
void calculate_frame_delta(unsigned char *frame, res_t *res, float *delta)
{
  int i, j, d=0, dd=0;
  timestamp_t start_time, stop_time;   /* runtimes for each pass */
  double exec_time_c=0.0, exec_time_avx=0.0;
  int pitch = 16;
  int n = res->width/pitch;
  int dd_avx=0;

  get_time(&start_time);
  for (i=0; i<(res->height-1); i++){
    for(j=0; j<res->width; j++) {
      d = frame[i*res->width + j]-frame[(i+1)*res->width + j];
      dd += d * d;     
    }            
  }
  get_time (&stop_time);
  exec_time_c = elapsed_time (&start_time, &stop_time);
  printf("dd_frame_norm: %-47d    norm_t: %f\n", dd, exec_time_c);

  get_time(&start_time);
  for (i=0; i<(res->height-1); i++){
    dd_avx += ssd_nx16_u8_avx2_intrin (&frame[i*res->width], &frame[(i+1)*res->width], pitch, n);
  }
  get_time (&stop_time);
  exec_time_avx = elapsed_time (&start_time, &stop_time);
  printf("dd_frame_avx2: %-47d    avx2_t: %f\n", dd_avx, exec_time_avx);

  *delta = (float)dd / ((res->height-1)*res->width);
}

void calculate_deltas(unsigned char *frame, res_t *res, float *delta, float *delta_even, float *delta_odd)
{
  calculate_field_delta(frame, res, delta_even, delta_odd);
  calculate_frame_delta(frame, res, delta);
}

/*!
 *  \brief Format detector program.
 * 
 *  Design notes: 
 * 
 * 
 */

int main (int argc, char* argv[])
{
  /* program parameters: */
  static char *input = NULL;             //!< input filename 
  static res_t resolution = {0,0};
  static fps_t framerate = {0,0};
  static int format = FORMAT_YUV420;     //!< default chroma format
  static char *temp_dir = NULL;                       // temporary directory
  static int bitdepth = 8;               //!< default bitdepth
  static int verbose = 0;

  /* frame buffers: */
  unsigned char *frame, *cur, *prev;

  /* deltas */
  float delta_frame;                  //current frame odd and even difference
  float delta_even_fields;                 //continuous frames even and even difference
  float delta_odd_fields;                 //continuous frames odd and odd difference

  FILE *input_file; 
  FILE *f_delta_log; 
  int result;

  /* create temporary dir and logs */
  int keepfolders = 0;             //!< default not keep log files under /tmp
  char dirname[STRLEN], delta_log[STRLEN];
  char *input_name, *filename;

  /* other vars: */
  int size, i;

  /* define tuning param gamma */
  float gamma=0.0;

  /* print program name & version */
  version ();

  /* parse command line: */
  read_command_line(argc, argv, &input, &resolution, &framerate, &format, &temp_dir, &bitdepth, &verbose);

  /* allocate frame buffers: */
  size = frame_size(&resolution, format, bitdepth);
  if (size <= 0) error (1, "Invalid video parameters.\n");
  frame = (unsigned char*) malloc(size);
  cur = (unsigned char*) malloc(size);
  prev = (unsigned char*) malloc(size);
  if (!cur || !prev) error(1, "Out of memory.\n"); 

  /* open input file: */
  if ((input_file = fopen(input, "rb")) == NULL) 
    error(1, "Cannot open file '%s'\n", input);

  /* print progress: */
  if (verbose) 
  {
    printf ("Processing:\n  >");
    keepfolders = 1;    // keep log files under debug mode
  }

  /* generate unique name for temp directory: */
  result = make_temp_dir (dirname, STRLEN, temp_dir);
  if (result) {
    error(0, "Cannot create temp directory\n");
    return result;
  }

  input_name = basename(input);
  input_name = remove_filename_extension(input_name);
  memset(delta_log, 0, STRLEN);
  sprintf(delta_log, "%s%c%s", dirname,  DIRSEP, input_name);

  filename = strcat(delta_log,".csv");
  f_delta_log = fopen(filename, "w");
  fprintf (f_delta_log, "\tdelta_frame,delta_even,delta_odd,gamma\n");

  /* main loop: */
  for (i=0; ; i++) 
  {

    /* read frame: */
    if (fread (frame, size, 1, input_file) != 1)
      break;

    calculate_deltas(frame, &resolution, &delta_frame, &delta_even_fields, &delta_odd_fields);
    gamma = delta_frame / (delta_even_fields + delta_odd_fields + 0.00001);
    fprintf (f_delta_log, "%8.5f,%8.5f,%8.5f,%8.5f\n", delta_frame, delta_even_fields, delta_odd_fields, gamma);

    /* print progress: */
    if (verbose && i > 0 && i % 10 == 0)
      printf(".");
  }

  fclose (f_delta_log);

  /* nuke all log files */
  if (!keepfolders) {
    _unlink(delta_log);
    _rmdir(dirname);
  }

  /* progress indicator: */
  if (verbose) {
    printf("<\n");
    printf("=> %d frames processed\n", i);
  }

  /* close files, free buffers & exit: */
  fclose(input_file); 
  free(prev); 
  free(cur); 
  return 0;
}