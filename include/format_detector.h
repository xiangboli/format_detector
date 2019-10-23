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

#ifndef _FORMAT_DETECTOR_H_
#define _FORMAT_DETECTOR_H_  1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VERSION
#define VERSION "1.0.0"
#endif

/* min/max macros: */
#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

/*
 * Constants, enums:  
 */
/* Global limits/constants: */
#define MAX_WIDTH                 8192        //!< max frame width
#define MAX_HEIGHT                8192        //!< max frame height
#define WINSIZE_HEIGHT            20          //!< max size of row sliding window, odd and even rows are half size
#define WINSIZE_WIDTH             10          //!< max size of column sliding window
#define BINS                      100         //!< numbed of bins in histogram
#define MIN_FIELD_DIFF            0           //!< min odd, even field difference 
#define MAX_FIELD_DIFF            0.5         //!< max odd, even field difference

/* line buffer length */
#define STRLEN  4096


/* Intel X86 SIMD Mask */
#define PREAVX2_MASK    1
#define AVX2_MASK       2
#ifndef NON_AVX512_SUPPORT
#define AVX512_MASK     4
#endif
#define ASM_AVX2_BIT    3

/*! Scan order type */
enum {
  SCAN_UNKNOWN = 0,
  SCAN_PROGRESSIVE = 1,
  SCAN_INTERLACE_TFF = 2, 
  SCAN_INTERLACE_BFF = 3
};

/*! Chroma sampling format */
enum {
  FORMAT_UNKNOWN = 0,
  FORMAT_YUV420 = 1,
  FORMAT_YUV422 = 2, 
  FORMAT_YUV444 = 3
};

/* 
 * Data types
 */

/*! Video resolution */
typedef struct {int width, height;} res_t;

/*! Video framerate */
typedef struct {int num, denom;} fps_t;

/* 
 * Function prototypes:
 */

/* implemented in format_detector_utils.c */
fps_t float_to_fps (float x);
float fps_to_float (fps_t fps);
float clamp (float x, float x_min, float x_max);
int make_temp_dir (char *final_dir, int bufsize, char *user_temp_dir);
int min_index (double *x, int n);
char *basename (char *name);
char *remove_filename_extension (char* mystr);

/* implemented in interlace_detector.c */
void detect_interlace(unsigned char *frame, res_t *res, char *hist_log);

/* implemented in telecine_detector.c */


/* Sum of abosulate difference (SAD) of nx8 windown with AVX2 intrinsic functions */
int sad_nx8_u8_avx2_intrin(unsigned char *p, unsigned char *q, int pitch, int n);  

/* Sum of squared difference (SSD) of nx8 windown with AVX2 intrinsic functions */
int ssd_nx8_u8_avx2_intrin(unsigned char *p, unsigned char *q, int pitch, int n);  

/* Sum of abosulate difference (SAD) of nx16 windown with AVX2 intrinsic functions */
int sad_nx16_u8_avx2_intrin(unsigned char *p, unsigned char *q, int pitch, int n);  

/* Sum of squared difference (SSD) of nx16 windown with AVX2 intrinsic functions */
int ssd_nx16_u8_avx2_intrin(unsigned char *p, unsigned char *q, int pitch, int n);  


#ifdef __cplusplus
}
#endif

#endif /* _FORMAT_DETECTOR_H_ */