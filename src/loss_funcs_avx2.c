#include <immintrin.h>
#include <stdio.h>

#include "format_detector.h"

/*!
 * @brief Calcalute sum of absolute difference between nx8 window with AVX2
 * 
 * @param p        1st array of data
 * @param q        2nd array of data
 * @param pitch    8 bytes length
 * @param n        n = len(array)/pitch
 * @return int 
 */
int sad_nx8_u8_avx2_intrin (unsigned char *p, unsigned char *q, int pitch, int n)
{
   __m256i vp, vq;
   int i; 
   int result[8]; 
   
   __m256i mask = _mm256_setr_epi64x(-13, 4, 8, 3);
   __m256i sad = _mm256_setzero_si256();
   for (i=0; i<n; i++) {
      vp = _mm256_maskload_epi64((long long *)(p+i*pitch), mask);
      vq = _mm256_maskload_epi64((long long *)(q+i*pitch), mask);
      sad = _mm256_add_epi32(sad, _mm256_sad_epu8(vp, vq));
   }
   _mm256_storeu_si256((__m256i*) &result[0], sad);

   return result[0];
}


/*!
 * @brief Calcalute sum of absolute difference between nx16 window with AVX2
 * 
 * @param p        1st array of data
 * @param q        2nd array of data
 * @param pitch    16 bytes length
 * @param n        n = len(array)/pitch
 * @return int 
 */
int sad_nx16_u8_avx2_intrin (unsigned char *p, unsigned char *q, int pitch, int n)
{
   __m256i vp, vq;
   int i;
   int result[8];   
   
   __m256i mask = _mm256_setr_epi64x(-13, -12, 8, 3);
   __m256i sad = _mm256_setzero_si256();
   __m256i zeros = _mm256_setzero_si256();
   for (i=0; i<n; i++) {
      vp = _mm256_maskload_epi64((long long *)(p+i*pitch), mask);
      vq = _mm256_maskload_epi64((long long *)(q+i*pitch), mask);
      sad = _mm256_add_epi32(sad, _mm256_sad_epu8(vp, vq));
   }
   
   sad =_mm256_hadd_epi32(_mm256_hadd_epi32(sad, zeros), zeros);
   _mm256_storeu_si256((__m256i*) &result[0], sad);

   return result[0];
}

/*!
 * @brief Calcalute sum of squared difference between nx8 window with AVX2
 * 
 * @param p        1st array of data
 * @param q        2nd array of data
 * @param pitch    8 bytes length
 * @param n        n = len(array)/pitch
 * @return int 
 */
int ssd_nx8_u8_avx2_intrin (unsigned char *p, unsigned char *q, int pitch, int n)
{
   __m256i vp, vq, va;
   int i;  
   int result[8]; 

   __m256i zeros = _mm256_setzero_si256();
   __m128i mask = _mm_set_epi64x(4, -13);
   __m256i ssd = _mm256_setzero_si256();
   for (i=0; i<n; i++) {
      vp =  _mm256_cvtepu8_epi16(_mm_maskload_epi64((long long *)(p+i*pitch), mask));
      vq =  _mm256_cvtepu8_epi16(_mm_maskload_epi64((long long *)(q+i*pitch), mask));
      va = _mm256_sub_epi16(vp, vq);
      ssd = _mm256_add_epi32(ssd, _mm256_madd_epi16(va, va));
   }

   ssd =_mm256_hadd_epi32(_mm256_hadd_epi32(ssd, zeros), zeros);
   _mm256_storeu_si256((__m256i*) &result[0], ssd);

   return result[0];
}


/*!
 * @brief Calcalute sum of squared difference between nx16 window with AVX2
 * 
 * @param p        1st array of data
 * @param q        2nd array of data
 * @param pitch    16 bytes length
 * @param n        n = len(array)/pitch
 * @return int 
 */
int ssd_nx16_u8_avx2_intrin (unsigned char *p, unsigned char *q, int pitch, int n)
{
   __m256i a, b, vp, vq, va, vp1, vq1, va1;
   int i;  
   int result[8]; 

   __m256i zeros = _mm256_setzero_si256();
   __m256i mask = _mm256_setr_epi64x(-13, -12, 8, 3);
   __m256i ssd = _mm256_setzero_si256();
   for (i=0; i<n; i++) {
      a = _mm256_maskload_epi64((long long *)(p+i*pitch), mask);
      b = _mm256_maskload_epi64((long long *)(q+i*pitch), mask);
      
      vp = _mm256_unpacklo_epi8(a, zeros);
      vq = _mm256_unpacklo_epi8(b, zeros);
      va = _mm256_sub_epi16(vp, vq);

      vp1 = _mm256_unpackhi_epi8(a, zeros);
      vq1 = _mm256_unpackhi_epi8(b, zeros);
      va1 = _mm256_sub_epi16(vp1, vq1);

      ssd = _mm256_add_epi32(ssd, _mm256_add_epi32(_mm256_madd_epi16(va, va), _mm256_madd_epi16(va1, va1)));
    }

    ssd =_mm256_hadd_epi32(_mm256_hadd_epi32(ssd, zeros), zeros);
    _mm256_storeu_si256((__m256i*) &result[0], ssd);

   return result[0];
}