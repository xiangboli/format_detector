/*!
 *  \file     timer.h
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

#ifndef _TIMER_
#define _TIMER_ 1

#ifdef _MSC_VER 
#include <windows.h>
#endif
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* time record */
typedef union {
  struct timespec time_spec;  
  long long cpu_counter;  
} timestamp_t;

/* functions: */
void get_time (timestamp_t *t);                             //!< records current time
double elapsed_time (timestamp_t *start, timestamp_t *end);	//!< computes time difference between 2 events [in milliseconds]

#ifdef __cplusplus
}
#endif

#endif /* _TIMER_ */
