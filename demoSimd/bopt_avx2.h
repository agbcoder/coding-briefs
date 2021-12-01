/* Optimization tests */

#ifndef __BOPT_AVX2_H__
#define __BOPT_AVX2_H__

#ifdef __cplusplus
extern "C" {
#endif


/*!
  * @brief Interleaves data from channels c0-c3 into a compound one of format c0c1c2c3. Think of RGB + alpha channels.
  * @param dst destination buffer, its size must be 4x each of the input buffers
  * @param c0 channel to be packed into     least significant byte. This one can be NULL to use fixed value 0xFF.
  * @param c1 channel to be packed into 2nd least significant byte
  * @param c2 channel to be packed into 3rd least significant byte
  * @param c3 channel to be packed into     most  significant byte
  * @param num_samples size of each single channel buffer
  * @return number of samples packed into destination (typically pixels copied)
  */
int channels_to_interleaved_8b(unsigned char *dst,
                               unsigned char *ch0, unsigned char *ch1, unsigned char *ch2, unsigned char *ch3,
                               int num_samples);


#ifdef __cplusplus
}
#endif

#endif
