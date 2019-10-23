/*!
 *  \file     pattern_detector_utils.c
 *  \brief    Scan pattern analysis application
 * 
 *  \version  1.0.00
 *  \date     Tue Feb. 5, 2019
 *
 *  \authors  Xiangbo Li
 *
 */

/* OS-specific definitions: */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#include <intrin.h>
#include <windows.h>
#include <direct.h>
#include <io.h>
#define	F_OK	0		// existence
#define	W_OK	2		// write permission
#define	R_OK	4		// read permission
#define DIRSEP '\\'
#else
#include <unistd.h>
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/syslimits.h>
#include <strings.h>
#else
#include <linux/limits.h> // needed for PATH_MAX
#endif
#include <sys/stat.h>
#include <stdarg.h> // va_list
#include <stdint.h>  //need for instruction set support
#define _access access
#define _mkdir(dir)  mkdir(dir, S_IRWXU | S_IRWXO | S_IRWXG)
#define DIRSEP '/'
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>

#include "pattern_detector.h"

/**************************************
* Instruction Set Support
**************************************/
void run_cpuid(unsigned int eax, unsigned int ecx, int* abcd)
{
#if defined(_MSC_VER)
    __cpuidex(abcd, eax, ecx);
#else
    uint32_t ebx, edx;
# if defined( __i386__ ) && defined ( __PIC__ )
     /* in case of PIC under 32-bit EBX cannot be clobbered */
    __asm__ ( "movl %%ebx, %%edi \n\t cpuid \n\t xchgl %%ebx, %%edi" : "=D" (ebx),
# else
    __asm__ ( "cpuid" : "=b" (ebx),
# endif
              "+a" (eax), "+c" (ecx), "=d" (edx) );
    abcd[0] = eax; abcd[1] = ebx; abcd[2] = ecx; abcd[3] = edx;
#endif
}

int check_4th_gen_intel_core_features()
{
    int abcd[4];
    int fma_movbe_osxsave_mask = ((1 << 12) | (1 << 22) | (1 << 27));
    int avx2_bmi12_mask = (1 << 5) | (1 << 3) | (1 << 8);
 
    /* CPUID.(EAX=01H, ECX=0H):ECX.FMA[bit 12]==1   && 
       CPUID.(EAX=01H, ECX=0H):ECX.MOVBE[bit 22]==1 && 
       CPUID.(EAX=01H, ECX=0H):ECX.OSXSAVE[bit 27]==1 */
    run_cpuid( 1, 0, abcd );
    if ( (abcd[2] & fma_movbe_osxsave_mask) != fma_movbe_osxsave_mask ) 
        return 0;
 
    /*  CPUID.(EAX=07H, ECX=0H):EBX.AVX2[bit 5]==1  &&
        CPUID.(EAX=07H, ECX=0H):EBX.BMI1[bit 3]==1  &&
        CPUID.(EAX=07H, ECX=0H):EBX.BMI2[bit 8]==1  */
    run_cpuid( 7, 0, abcd );
    if ( (abcd[1] & avx2_bmi12_mask) != avx2_bmi12_mask ) 
        return 0;
    /* CPUID.(EAX=80000001H):ECX.LZCNT[bit 5]==1 */
    run_cpuid( 0x80000001, 0, abcd );
    if ( (abcd[2] & (1 << 5)) == 0)
        return 0;
    return 1;
}
static int can_use_intel_core_4th_gen_features()
{
    static int the_4th_gen_features_available = -1;
    /* test is performed once */
    if (the_4th_gen_features_available < 0 )
        the_4th_gen_features_available = check_4th_gen_intel_core_features();
    return the_4th_gen_features_available;
}

// Returns ASM Type based on system configuration. AVX512 - 111, AVX2 - 011, NONAVX2 - 001, C - 000
// Using bit-fields, the fastest function will always be selected based on the available functions in the function arrays
unsigned int get_cpu_asm_type()
{
	unsigned int asm_type = 0;
  if (can_use_intel_core_4th_gen_features() == 1){
      asm_type = 3; // bit-field
  }
  else{
      asm_type = 1; // bit-field
  }
  
	return asm_type;
}

/*!
 *  \brief Make a temporaty directory to work with
 *
 *  \param[in,out] final_dir     - pointer to buffer where final accessable temp directory's path will be stored
 *  \param[in]     bufsize       - size of final_dir buffer
 *  \param[in]     user_temp_dir - user provided temporary directory (can be NULL)
 *
 *  \returns    0 if success, !0 if error
 */
int make_temp_dir (char *final_dir, int bufsize, char *user_temp_dir)
{
  int result = 0;

  /* clear output buffer */
  memset(final_dir, 0, bufsize);

  /* See if given directory is accessable */
  if (!user_temp_dir || _access(user_temp_dir, W_OK))  // _access() returns 0 if accessable
  {
    #ifdef _MSC_VER
      wchar_t w_final_dir[MAX_PATH];

      /* get the temp path from TEMP variable: */
      GetTempPath(MAX_PATH, w_final_dir);
      wcstombs(final_dir, w_final_dir, min(MAX_PATH,bufsize));

      /* check if the path is writable: */
      if(_access(final_dir, W_OK)) return -1;

      /* append the custom string */
      result = _mkdir(final_dir);
      if (result < 0 && errno == EEXIST)
        result = 0; // do not error if directory already exists
    #else
      sprintf (final_dir, "/tmp/format_XXXXXX");  // should end with 6 Xs for mkdtemp to work
      mkdtemp(final_dir);
      result = _access(final_dir, W_OK); // See if temp dir created and check the access
    #endif
  }
  else
  {
    sprintf (final_dir, "%s%cs", user_temp_dir, DIRSEP);
    result = _mkdir(final_dir);
    if (result < 0 && errno == EEXIST)
      result = 0; // do not error if directory already exists
  }

  /* return error code: */
  return result;
}



/*!
 *  \brief Clamps value x to be in range [x_min, x_max].
 *
 *  \param[in] x      - variable
 *  \param[in] x_min  - lower limit
 *  \param[in] x_max  - upper limit
 *
 *  \returns  x constrained to [x_min,x_max] range
*/
float clamp(float x, float x_min, float x_max)
{
  return (x < x_min)? x_min: (x > x_max)? x_max: x;
}


/********************************************
 *  
 *  Operations with framerates:
 *
 *   float_to_fps ()
 *   fps_to_float ()
 *   cmp_fps()
 *
 ********************************************/

 /*!
 *  \brief Floating point number -> framerate conversion.
 *
 *  Background: most video framerates are either integers or fractions with 1001 in the denominator.
 *  We basically need to figure out which one it is based on limited (2-3 decimal digits) floating point representation.
 */
fps_t float_to_fps (float x)
{
  fps_t fps;
  /* standard x / 1001 framerates: */
  if (fabs(x - 23.976) < 0.001) {fps.num = 24000;  fps.denom = 1001;} else
  if (fabs(x - 29.97)  < 0.01)  {fps.num = 30000;  fps.denom = 1001;} else
  if (fabs(x - 47.952) < 0.001) {fps.num = 48000;  fps.denom = 1001;} else
  if (fabs(x - 59.94)  < 0.01)  {fps.num = 60000;  fps.denom = 1001;} else
  if (fabs(x - 119.88) < 0.01)  {fps.num = 120000; fps.denom = 1001;} else
  /* standard integral rates: */
  if (fabs(x - 24.0)  < 0.01)   {fps.num = 24;     fps.denom = 1;}    else
  if (fabs(x - 25.0)  < 0.01)   {fps.num = 25;     fps.denom = 1;}    else
  if (fabs(x - 30.0)  < 0.01)   {fps.num = 30;     fps.denom = 1;}    else
  if (fabs(x - 48.0)  < 0.01)   {fps.num = 48;     fps.denom = 1;}    else
  if (fabs(x - 50.0)  < 0.01)   {fps.num = 50;     fps.denom = 1;}    else
  if (fabs(x - 60.0)  < 0.01)   {fps.num = 60;     fps.denom = 1;}    else
  if (fabs(x - 96.0)  < 0.01)   {fps.num = 96;     fps.denom = 1;}    else
  if (fabs(x - 100.0) < 0.01)   {fps.num = 100;    fps.denom = 1;}    else
  if (fabs(x - 120.0) < 0.01)   {fps.num = 120;    fps.denom = 1;}    else
  /* everything else: */
  {fps.num = (int)floor(x * 100000 + 0.5); fps.denom = 100000;}
  return fps;
}

/*! Framerate -> floating point number conversion. */
float fps_to_float (fps_t fps)
{
  return (fps.num == 0)? 0: (float)fps.num / (float)fps.denom;
}


/*
 * Find index of minimum value in array.
 */
int min_index (double *x, int n)
{
  register int i, i_min=0;
  register double x_min = x[0];
  for (i=1; i<n; i++) if (x[i] < x_min) {x_min = x[i]; i_min = i;}
  return i_min;
}


#ifdef _MSC_VER
/*!
 *  \brief Extract filename portion of pathname, i.e. component after last directory separator
 */
char *basename (char *name)
{
  char *p = strrchr(name, DIRSEP);
  return (p != NULL)? p+1: name;
}
#endif


/* Remove extension from filename */
char *remove_filename_extension (char* mystr)
{
  char *retstr, *lastdot, *lastsep;

  // Error checks and allocate string.
  if (mystr == NULL) return NULL;
  if ((retstr = malloc (strlen (mystr) + 1)) == NULL) return NULL;

  // Make a copy and find the relevant characters.
  strcpy (retstr, mystr);
  lastdot = strrchr (retstr, '.');
  lastsep = strrchr (retstr, DIRSEP);

  // If it has an extension separator.
  if (lastdot != NULL) {
    // and it's before the extension separator.
    if (lastsep != NULL) {
      if (lastsep < lastdot) {
        // then remove it.
        *lastdot = '\0';
      }
    } else {
      // Has extension separator with no path separator.
      *lastdot = '\0';
    }
  }

  // Return the modified string.
  return retstr;
}
