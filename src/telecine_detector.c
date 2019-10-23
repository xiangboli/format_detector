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

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define strcasecmp _stricmp
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "getopt.h"
#include "format_detector.h"


/*!
 *  \brief Compute sums of square differences between 2 successive frames/fields. 
 * 
 *  Note: this function computes SSDs only for luma portions of frames. 
 *
 *  \param[in]    cur         - pointer to current frame
 *  \param[in]    prev        - pointer to previous frame
 *  \param[in]    res         - resolution
 * 
 *  \param[out]   ssd_even    - sum of square differences for even fields
 *  \param[out]   ssd_odd     - sum of square differences for odd fields
 * 
 *  \returns      none
 */
void compute_ssd (unsigned char *cur, unsigned char *prev, res_t *res, double *ssd_even, double *ssd_odd)
{
  int a, d, i, j;
  int s_even = 0, s_odd = 0;

  /* scan image: */
  for (i=0; i<res->height; i++) 
  {
    /* compute sum of differences for a row: */
    for (j=0,a=0; j<res->width; j++) 
    {
      d = prev[i*res->width + j] - cur[i*res->width + j];
      a += d*d;
    }

    /* add it to field counters: */
    if (i & 1)  s_odd += a;
    else        s_even += a;
  }

  /* normalize & store the results: */
  d = res->width * res->height/2;                 // field size
  *ssd_even = (double) s_even / (double) d;
  *ssd_odd = (double) s_odd / (double) d;
}

/*!
 *  \brief Telecine detector program.
 * 
 *  Design notes: 
 *
 *   most common US telecine patterns are:
 *    2:3 pattern, which is 2-3-2-3, or assuming 4 frames A,B,C,D, it is a sequence A0:A1, B0:B1, C0:B1, D0:C1, D0:D1, or
 *    3:2 pattern, which is 3-2-3-2, or assuming 4 frames A,B,C,D, it is a sequence A0:A1, B0:A1, C0:B1, C0:C1, D0:D1,
 *   where 0 denote top/even and 1 bottom/odd fields, and where we assume that top filed is first.
 *   For bottom field first, it is slightly different. 
 *
 *   Both patterns are effectively same thing, just shifted by 1 frame. 
 *   In practice we may have many other combinations due to the fact that we don't know at which point the pattern starts and which field is first. 
 * 
 *   To detect patterns we will be looking at pair-wise differences between even and odd fields in sequence of 5 adjustent frames.
 *   In case of classic 2:3 pattern, this will produce:
 *             0       1       2        3          4       <- temporal offset 
 *    even:  D0-A0   B0-A0    C0-B0     0 (D0-D0) A0-D0
 *    odd:   D1-A1   B1-A1    0 (B1-B1) D1-C1     A1-D1
 *
 *   where it is clear that if 2:3 pattern is present then one of the differences across each field differences will be zero. 
 *
 *   In cases of 2:3 and 3:2 pulldowns zero-differences in even and odd fields will be in different locations. 
 *   In case of Panasonic 2:3:3:2 pulldown or some other pulldown patterns, they may actually coinside.
 * 
 *   For detecting Euro 2:2:2:2:2:2:2:2:2:2:2:3 pulldown a chain of 25 frame differences is needed. 
 *   In this case 0 difference will be observed only for 1 field. 
 *
 *   Generally, telecine may be present and have consistent cadence across entire sequence, or it may be present in a segment, 
 *   or its cadence may change from segment to segment. The task of detector is to figure our which one of these scenaria applies.
 *
 *   If the above described field-difference analysis results in near perfect zeros for entire sequence - this indicates that telecine pattern is fixed. 
 *   However, if telecine varies or present only in chunk - the differences for entire sequence won't catch it. 
 *   To detect local appearances of telecine in a segment of video, we will need to run above discribed difference analysis in a sliding window. 
 *   For reliable results, the length of such window has to be at least about 10x the period of telecine - which implies at least 50 frames for 3:2 pulldown detection. 
 *   When telecine pattern is detected in such local window - the detector program should note it, and then output its type and duration within a sequence. 
 *
 */
int telecine_detector(int argc, char* argv[])
{
  /* program parameters: */
  char *input = NULL;             //!< input filename
  FILE *input_file;
  res_t resolution = {0,0};
  fps_t framerate = {0,0};
  int interlace = SCAN_UNKNOWN;   //!< scan order
  int format = FORMAT_YUV420;     //!< default chroma format
  int bitdepth = 8;               //!< default bitdepth
  int verbose = 0;
  int y4m = 0;                    //!< detected based on filename or start header

  /* frame buffers: */
  unsigned char *cur, *prev;

  /* telecine pattern length: */
  #define PATTERN_LENGTH 5        //!< only good for 3:2 pulldowns
                                  
  /* even/odd field SSD tables: */
  double sequence_ssd_even [PATTERN_LENGTH]; 
  double sequence_ssd_odd [PATTERN_LENGTH];
  double ssd_even, ssd_odd;

  /* other vars: */
  int size, i, j, k, pi, pj;

  /* print program name & version */
  version ();

  /* parse command line: */
  read_command_line(argc, argv, &input, &resolution, &framerate, &format, &bitdepth, &interlace, &verbose);

  /* allocate frame buffers: */
  size = frame_size(&resolution, format, bitdepth);
  if (size <= 0) error (1, "Invalid video parameters.\n");
  cur = (unsigned char*) malloc(size);
  prev = (unsigned char*) malloc(size);
  if (!cur || !prev) error(1, "Out of memory.\n"); 

  /* open input file: */
  if ((input_file = fopen(input, "rb")) == NULL) 
    error(1, "Cannot open file '%s'\n", input);

  /* check if file .y4m file 
    if (...)
      y4m = 1;
   */

  /* zero SSD tables: */
  for (k = 0; k < PATTERN_LENGTH; k ++) {
    sequence_ssd_even[k] = 0;
    sequence_ssd_odd[k] = 0;
  }

  /* print progress: */
  if (verbose) 
    printf ("Processing:\n  >");

  /* main loop: */
  for (i=0, j=0; ; i++) 
  {
    /* skip y4m header.. 
      if (y4m)
         fgets(...);
    */

    /* read frame: */
    if (fread (cur, size, 1, input_file) != 1)
      break;

    /* collect stats for telecine detection: */
    if (i > 0) 
    {
      /* compute mean square differences between top and bottom fields: */
      compute_ssd (cur, prev, &resolution, &ssd_even, &ssd_odd);

      /* add them to total counters: */
      sequence_ssd_even[i % PATTERN_LENGTH] += ssd_even;
      sequence_ssd_odd[i % PATTERN_LENGTH] += ssd_odd;
    }

    /* save current frame: */
    memcpy(prev, cur, size);

    /* print progress: */
    if (verbose && i > 0 && i % 10 == 0)
      printf(".");
  }

  /* progress indicator: */
  if (verbose) {
    printf("<\n");
    printf("=> %d frames processed\n", i);
  }

  /* normalize SSD values: */
  for (k=0, ssd_even = 0., ssd_odd = 0.; k<PATTERN_LENGTH; k++) {ssd_even += sequence_ssd_even[k]; ssd_odd += sequence_ssd_odd[k];}
  for (k=0; k<PATTERN_LENGTH; k++) {sequence_ssd_even[k] /= ssd_even; sequence_ssd_odd[k] /= ssd_odd;}

  /* show distributions: */
  if (verbose) {
    printf("\nSSDs for even fields: ");
    for (k=0; k<PATTERN_LENGTH; k++) printf("%.3lf, ", sequence_ssd_even[k]); 
    printf("\nSSDs for odd fields:  ");
    for (k=0; k<PATTERN_LENGTH; k++) printf("%.3lf, ", sequence_ssd_odd[k]); 
    printf("\n\n");
  }

  /* find offsets resulting in minimum SSDs: */
  i = min_index (sequence_ssd_even, PATTERN_LENGTH);
  j = min_index (sequence_ssd_odd, PATTERN_LENGTH);

  /* check if minimum SSDs are low enought:
   *   Note: these limits will work only for "clean" telecine. 
   *   For deinterlaced or transcoded content this detection really needs to be more creative.
   *   We probably should look at energies of all frames, and/or run motion comp and see energy of residual in static regions to be able to infer amount of noise added. */
  if (sequence_ssd_even[i] >= 0.1 || sequence_ssd_odd[j] >= 0.1) 
  {
    /* use verbose dump */
    if (verbose) printf("=> sequence does not seem to have a clear Telecine pattern\n");
    /* print negative detection result: */
    printf("Telecine IS NOT detected\n");
  } 
  else 
  {
    /* print positive detection result: */
    printf("Telecine detected\n");

    /* figure out type of telecine pattern and print it .... */
  }

  /* close files, free buffers & exit: */
  fclose(input_file); 
  free(prev); 
  free(cur);
  return 0;
}

