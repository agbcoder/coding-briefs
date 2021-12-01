/* Optimization tests */

#ifndef __BOPT_AVX2_PRIV_H__
#define __BOPT_AVX2_PRIV_H__

#ifdef __cplusplus
extern "C" {
#endif

/** Private headers for the tests */

int channels_ileaved_d32_s32_n32m_8b_intrinsics(
    unsigned char *dst,
    unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
    int num_pixels);

int channels_ileaved_dmis_smis_n32m_8b_intrinsics(
        unsigned char *dst,
        unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
        int num_pixels);

int channels_ileaved_dmis_smis_n16m_8b_intrinsics(
        unsigned char *dst,
        unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
        int num_pixels);

int channels_ileaved_dmis_smis_n08m_8b_intrinsics(
        unsigned char *dst,
        unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
        int num_pixels);

#ifdef __cplusplus
}
#endif

#endif
