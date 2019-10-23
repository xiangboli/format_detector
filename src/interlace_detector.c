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
 *  \brief Detect interlace by computing differences between odd and even fields in a frame. 
 *  
 *  Note: this function computes difference only for luma portions of frames. 
 *
 *  \param[in]    frame       - pointer to current frame
 *  \param[in]    res         - resolution
 * 
 *  \param[out]   hist_log    - log histgram distribution for field difference
 * 
 *  \returns      none
 */

void detect_interlace(unsigned char *frame, res_t *res, char *hist_log)
{
  /* sliding window and histogram: */
  int i, j, m, r, c, k, step;
  int energe_even, energe_odd, d[WINSIZE_HEIGHT/2][WINSIZE_WIDTH], pcf_even[WINSIZE_HEIGHT/2][WINSIZE_WIDTH], pcf_odd[WINSIZE_HEIGHT/2][WINSIZE_WIDTH];
  int delta, hist[BINS], hist_count=0;
  float field_diff_origin, field_diff_norm, hist_total=0.0, field_diff[BINS];

  FILE *f_hist_log;

  /* zero histogram, window: */
  memset(hist, 0, sizeof(hist));
  memset(field_diff, 0, sizeof(field_diff));
  memset(d, 0, sizeof(d[0][0])*WINSIZE_HEIGHT/2*WINSIZE_WIDTH);
  memset(pcf_even, 0, sizeof(pcf_even[0][0])*WINSIZE_HEIGHT/2*WINSIZE_WIDTH);
  memset(pcf_odd, 0, sizeof(pcf_odd[0][0])*WINSIZE_HEIGHT/2*WINSIZE_WIDTH);

  /* scan image and build sliding window field difference histogram: */
  for (i=0; i<res->height-WINSIZE_HEIGHT; i++)  // i = current row #, j = current column #
  {
    /* update sliding window: */
    for(j=0,step=0,delta=0,energe_odd=0,energe_even=0; j<res->width-WINSIZE_WIDTH;)  // step = step # moving toward right
    {
      for (m=0; m<WINSIZE_HEIGHT; m++){
        if (step == 0) j=0;
        else j = step+WINSIZE_WIDTH-1;           // reset j index for each time slide window right one step
        do{
          r = m / 2;                             // recalculate the row index for odd and even 2d array.
          /* compute sum of a row inside the sliding window: */
          c = j % WINSIZE_WIDTH;                 // c - position in sliding window
          if(m & 1)
          {
            if (j >= WINSIZE_WIDTH) energe_odd -= pcf_odd[r][c];    // subtract outgoing value from running total
            pcf_odd[r][c] = frame[(m+i)*res->width + j];
            energe_odd += pcf_odd[r][c]*pcf_odd[r][c];

            /*  compute sum of differences between two field rows */
            if (j >= WINSIZE_WIDTH) delta -= d[r][c]*d[r][c];
            d[r][c] = pcf_odd[r][c] - pcf_even[r][c];
            delta += d[r][c]*d[r][c];

          }else{
            if (j >= WINSIZE_WIDTH) energe_even -= pcf_even[r][c]*pcf_even[r][c];    // subtract outgoing value from running total
            pcf_even[r][c] = frame[(m+i)*res->width + j];
            energe_even += pcf_even[r][c]*pcf_even[r][c];
          }
          j++;
        }while(j<WINSIZE_WIDTH);
      }
      step++;

      /* update histogram: */
      if (j >= WINSIZE_WIDTH) {      
        field_diff_origin = (float)delta/(energe_even + energe_odd);
        field_diff_origin = clamp(field_diff_origin, MIN_FIELD_DIFF, MAX_FIELD_DIFF);             // clamp to mappable range
        field_diff_norm = (field_diff_origin-MIN_FIELD_DIFF) / (MAX_FIELD_DIFF-MIN_FIELD_DIFF);   // get relative offset in mappable range
        k = (int) floor(field_diff_norm * (BINS-1) + 0.5f);                                       // compute bin #
        k = min(k, BINS-1);                                                                       // map ssim=1.0 into last bin  
        hist[k]++;         
        hist_count++;
        hist_total += field_diff_origin;                                                                      // update histogram
        field_diff[k] = field_diff_origin;                                                        // update field difference list      
      }  
    }
  }

  hist_log=strcat(hist_log,".csv");
  f_hist_log = fopen(hist_log, "w");
  fprintf (f_hist_log, "Average even and odd field difference in current frame: %.5f\n", (float)hist_total/hist_count);
  fprintf (f_hist_log, "fd_origin,fd_norm,dist(%%),hist\n");
  for(k=0; k<BINS-1; k++)
  {
    fprintf (f_hist_log, "%8.5f,%3d,%9.5f,%7d\n", field_diff[k], k, (float)hist[k]/hist_count*100, hist[k]);
  }

  fclose (f_hist_log);
}
