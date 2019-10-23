/*!
 *  \file     timer.c
 *  \brief    Cross-platform high-accuracy timer.
 *
 *  Cross-platform high-accuracy timer library. 
 * 
 *  \version  1.0.0
 *  \date     April 14, 2017
 *  \author   Yuriy Reznik
 *
 *  \copyright (c) 2017 Brightcove, Inc. 
 */

#ifdef _MSC_VER 
#include <windows.h>
#else
#include <time.h>
#endif

#include "timer.h"

/*!
 *  \brief Read system time 
 *
 *  \param[out]  t   - pointer to timestamp_t structure to contain record of system time
 */
void get_time (timestamp_t *t)
{
#ifdef _MSC_VER 
  QueryPerformanceCounter ((PLARGE_INTEGER)&t->cpu_counter);
#else
  clock_gettime(CLOCK_REALTIME, (struct timespec*)&t->time_spec);
#endif
}

/*!
 *  \brief  Compute elapsed time between two events.
 * 
 *  \param[in]  start  - start time
 *  \param[in]  end    - end time
 * 
 *  \returns    elapsed time [in milliseconds]
 */
double elapsed_time (timestamp_t *start, timestamp_t *end)
{
#ifdef _MSC_VER
  long long freq;
  QueryPerformanceFrequency ((PLARGE_INTEGER)&freq);
  return (double) (end->cpu_counter - start->cpu_counter) / (double) freq;
#else
  time_t sec  = end->time_spec.tv_sec - start->time_spec.tv_sec;
  long   nsec = end->time_spec.tv_nsec - start->time_spec.tv_nsec;
  if (nsec < 0) {sec --; nsec += 1000000000;} 
  return (double)sec + (double)nsec / 1000000000.;
#endif
}

/* timer.c -- end of file */
