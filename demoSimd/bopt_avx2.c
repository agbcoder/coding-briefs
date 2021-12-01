/* Intrinsic optimization tests */

#include "bopt_avx2.h"

#include <immintrin.h>
#include <assert.h>

/*!
  * @brief channels to interleaved, all pointers aligned to 32 AND num_samples multiple of 32.
  * @remark channels must be 8-bit per channel
  * @remark ALL input pointers MUST BE 32-byte aligned. Not checked.
  * @remark num_pixels MUST BE multiple of 32. Barely asserted.
  */
int channels_ileaved_d32_s32_n32m_8b_intrinsics(
    unsigned char *dst,
    unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
    int num_pixels)
{
    const int pixels_per_iteration = sizeof( __m256i ) / 1;     // 1 byte per pixel on each channel
    const int num_iterations = num_pixels / pixels_per_iteration;
    assert( !(num_pixels % pixels_per_iteration) );

    const __m256i *pr = (const __m256i *) r;
    const __m256i *pg = (const __m256i *) g;
    const __m256i *pb = (const __m256i *) b;
    const __m256i *pa = (const __m256i *) a;

    __m256i *pdst = (__m256i *) dst;
    __m256i rs, gs, bs, as;                 // each one an AVX value of all R, all G, all B, all A
    __m256i ar, gb;                         // pairs AR and GB
    __m256i argbT0, argbT1;                 // tmps to shuffle bytes
    __m256i argb0, argb1, argb2, argb3;     // 4*32 bytes interleaved result

    if ( !a ) as = _mm256_set1_epi8( 0xFF );// set fixed alpha value, if applies
    int num_pixels_copied = 0;
    for(int i=0; i<num_iterations; i++) {
        rs = _mm256_load_si256( pr );       // 32 bytes of R-channel
        gs = _mm256_load_si256( pg );
        bs = _mm256_load_si256( pb );
        if ( a ) as = _mm256_load_si256( pa );

        ar = _mm256_unpacklo_epi8( as, rs );    // low parts
        gb = _mm256_unpacklo_epi8( gs, bs );

        argbT0 = _mm256_unpacklo_epi16( ar, gb );
        argbT1 = _mm256_unpackhi_epi16( ar, gb );
        argb0  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 ); // take low  128 bits of each
        argb2  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 ); // take high 128 bits of each

        ar = _mm256_unpackhi_epi8( as, rs );   // high parts
        gb = _mm256_unpackhi_epi8( gs, bs );

        argbT0 = _mm256_unpacklo_epi16( ar, gb );
        argbT1 = _mm256_unpackhi_epi16( ar, gb );
        argb1  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 );
        argb3  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 );

        _mm256_store_si256( pdst++, argb0 );
        _mm256_store_si256( pdst++, argb1 );
        _mm256_store_si256( pdst++, argb2 );
        _mm256_store_si256( pdst++, argb3 );

        pr++; pg++; pb++;
        pa++;       // this one ignored if fixed alpha, but faster than checking
        num_pixels_copied += pixels_per_iteration;
    }

    return num_pixels_copied;
}

/*!
  * @brief channels to interleaved, no alignment at all, num_samples multiple of 32.
  * @remark channels must be 8-bit per channel
  * @remark num_pixels MUST BE multiple of 32. Barely asserted.
  */
int channels_ileaved_dmis_smis_n32m_8b_intrinsics(
    unsigned char *dst,
    unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
    int num_pixels)
{
    const int pixels_per_iteration = sizeof( __m256i ) / 1;     // 1 byte per pixel on each channel
    const int num_iterations = num_pixels / pixels_per_iteration;
    assert( !(num_pixels % pixels_per_iteration) );

    const __m256i *pr = (const __m256i *) r;
    const __m256i *pg = (const __m256i *) g;
    const __m256i *pb = (const __m256i *) b;
    const __m256i *pa = (const __m256i *) a;

    __m256i *pdst = (__m256i *) dst;
    __m256i rs, gs, bs, as;                 // each one an AVX value of all R, all G, all B, all A
    __m256i ar, gb;                         // pairs AR and GB
    __m256i argbT0, argbT1;                 // tmps to shuffle bytes
    __m256i argb0, argb1, argb2, argb3;     // 4*32 bytes interleaved result

    if ( !a ) as = _mm256_set1_epi8( 0xFF );// set fixed alpha value, if applies
    int num_pixels_copied = 0;
    for(int i=0; i<num_iterations; i++) {
        rs = _mm256_loadu_si256( pr );       // 32 bytes of R-channel
        gs = _mm256_loadu_si256( pg );
        bs = _mm256_loadu_si256( pb );
        if ( a ) as = _mm256_loadu_si256( pa );

        ar = _mm256_unpacklo_epi8( as, rs );    // even parts
        gb = _mm256_unpacklo_epi8( gs, bs );

        argbT0 = _mm256_unpacklo_epi16( ar, gb );
        argbT1 = _mm256_unpackhi_epi16( ar, gb );
        argb0  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 ); // take low  128 bits of each
        argb2  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 ); // take high 128 bits of each

        ar = _mm256_unpackhi_epi8( as, rs );   // odd parts
        gb = _mm256_unpackhi_epi8( gs, bs );

        argbT0 = _mm256_unpacklo_epi16( ar, gb );
        argbT1 = _mm256_unpackhi_epi16( ar, gb );
        argb1  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 );
        argb3  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 );

        _mm256_storeu_si256( pdst++, argb0 );
        _mm256_storeu_si256( pdst++, argb1 );
        _mm256_storeu_si256( pdst++, argb2 );
        _mm256_storeu_si256( pdst++, argb3 );

        pr++; pg++; pb++;
        pa++;       // this one ignored if fixed alpha, but faster than checking
        num_pixels_copied += pixels_per_iteration;
    }

    return num_pixels_copied;
}

/*!
  * @brief channels to interleaved, no alignment at all, num_samples multiple of 16.
  * @remark channels must be 8-bit per channel
  * @remark num_pixels MUST BE multiple of 16. Barely asserted.
  * @remark this function usually called with num_pixels=16, but make it up to multiples anyway
  */
int channels_ileaved_dmis_smis_n16m_8b_intrinsics(
    unsigned char *dst,
    unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
    int num_pixels)
{
    const int pixels_per_iteration = sizeof( __m128i ) / 1;     // 1 byte per pixel on each channel
    const int num_iterations = num_pixels / pixels_per_iteration;
    assert( !(num_pixels % pixels_per_iteration) );

    const __m128i *pr = (const __m128i *) r;
    const __m128i *pg = (const __m128i *) g;
    const __m128i *pb = (const __m128i *) b;
    const __m128i *pa = (const __m128i *) a;

    // Though only unpacking 16-byte teases, after first unpack we can rely on 256-bit instructions

    __m256i *pdst = (__m256i *) dst;
    __m128i rs16, gs16, bs16, as16;     // each one an SSE value of all R, all G, all B, all A
    __m256i rs, gs, bs, as;             // above values stored in 256 bits with upper 128 bits trash
    __m256i ar, gb;                         // pairs AR and GB
    __m256i argbT0, argbT1;                 // tmps to shuffle bytes
    __m256i argb0, argb1; //argb2, argb3;   // 2*32 (=4*16) bytes interleaved result. 2&3 not needed

    if ( !a ) as = _mm256_set1_epi8( 0xFF );// set fixed alpha value, if applies. Use 256-bit directly
    int num_pixels_copied = 0;
    for(int i=0; i<num_iterations; i++) {
        rs16 = _mm_loadu_si128( pr );           // 16 bytes of R-channel
        gs16 = _mm_loadu_si128( pg );
        bs16 = _mm_loadu_si128( pb );

        rs = _mm256_castsi128_si256( rs16 );    // stored them as lower 16 bytes of 32-byte registers
        gs = _mm256_castsi128_si256( gs16 );
        bs = _mm256_castsi128_si256( bs16 );

        if ( a ) {
            as16 = _mm_loadu_si128( pa );
            as   = _mm256_castsi128_si256( as16 );
        }

        // From this point on proceed as the 32-byte version, but all higher halves are discardable

        ar = _mm256_unpacklo_epi8( as, rs );    // even parts
        gb = _mm256_unpacklo_epi8( gs, bs );

        argbT0 = _mm256_unpacklo_epi16( ar, gb );
        argbT1 = _mm256_unpackhi_epi16( ar, gb );
        argb0  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 ); // take low  128 bits of each
        //argb2  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 ); // discard high 128 bits of each

        ar = _mm256_unpackhi_epi8( as, rs );    // odd parts
        gb = _mm256_unpackhi_epi8( gs, bs );

        argbT0 = _mm256_unpacklo_epi16( ar, gb );
        argbT1 = _mm256_unpackhi_epi16( ar, gb );
        argb1  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 );
        //argb3  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 );

        _mm256_storeu_si256( pdst++, argb0 );
        _mm256_storeu_si256( pdst++, argb1 );
        //_mm256_storeu_si256( pdst++, argb2 );
        //_mm256_storeu_si256( pdst++, argb3 );

        pr++; pg++; pb++;
        pa++;       // this one ignored if fixed alpha, but faster than checking
        num_pixels_copied += pixels_per_iteration;
    }

    return num_pixels_copied;
}

/*!
  * @brief channels to interleaved, no alignment at all, num_samples multiple of 8.
  * @remark channels must be 8-bit per channel
  * @remark num_pixels MUST BE multiple of 8. Barely asserted.
  * @remark This function usually called with num_pixels=8, but make it up to multiples anyway.
  * @remark The unaligned _loadu_ seems to be slower for this case than other bit-wider.
  */
int channels_ileaved_dmis_smis_n08m_8b_intrinsics(
    unsigned char *dst,
    unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
    int num_pixels)
{
    const int pixels_per_iteration = sizeof( __m64 ) / 1;      // 1 byte per pixel on each channel
    const int num_iterations = num_pixels / pixels_per_iteration;
    assert( !(num_pixels % pixels_per_iteration) );

    const unsigned char *pr = r, *pg = g, *pb = b, *pa = a;

    // Though only unpacking 8-byte teases, after first unpack we can rely on 256-bit instructions

    __m256i *pdst = (__m256i *) dst;
    __m128i rs8, gs8, bs8, as8;         // each one a 64-bit SSE value of all R, all G.. in lower half
    __m256i rs, gs, bs, as;             // above values stored in 256 bits with upper 128 bits trash
    __m256i ar, gb;                         // pairs AR and GB
    __m256i argbT0, argbT1;                 // tmps to shuffle bytes
    __m256i argb0; //argb1, argb2, argb3;   // 1*32 (=4*8) bytes interleaved result. 1-3 not needed

    if ( !a ) as = _mm256_set1_epi8( 0xFF );// set fixed alpha value, if applies. Use 256-bit directly
    int num_pixels_copied = 0;
    for(int i=0; i<num_iterations; i++) {
        rs8 = _mm_loadl_epi64( (const __m128i *) pr );      // 8 bytes of R-channel
        gs8 = _mm_loadl_epi64( (const __m128i *) pg );
        bs8 = _mm_loadl_epi64( (const __m128i *) pb );

        rs = _mm256_castsi128_si256( rs8 );     // stored them as lower 8 bytes of 32-byte registers
        gs = _mm256_castsi128_si256( gs8 );
        bs = _mm256_castsi128_si256( bs8 );

        if ( a ) {
            as8 = _mm_loadl_epi64( (const __m128i *) pa );
            as  = _mm256_castsi128_si256( as8 );
        }

        // From this point on proceed as the 32-byte version, but all 24 high BYTES are discardable

        ar = _mm256_unpacklo_epi8( as, rs );    // even parts; they include all lowest 8 bytes
        gb = _mm256_unpacklo_epi8( gs, bs );

        argbT0 = _mm256_unpacklo_epi16( ar, gb );
        argbT1 = _mm256_unpackhi_epi16( ar, gb );
        argb0  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 ); // take low  128 bits of each
        //argb2  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 ); // discard high 128 bits of each

        // All the 16-high bytes processing can be just skipped

        _mm256_storeu_si256( pdst++, argb0 );

        pr += sizeof( __m64 );      // advance only 64 bits !!
        pg += sizeof( __m64 );
        pb += sizeof( __m64 );
        pa += sizeof( __m64 );      // this one ignored if fixed alpha, but faster than checking
        num_pixels_copied += pixels_per_iteration;
    }

    return num_pixels_copied;
}

/* Pack 4 single channels into a packed interleaved format.
 *
 * Alignment is CRITICAL in any vectorized function. For the AVX2 instruction set
 * 32-bit alignment is the nominal one when possible.
 *
 * Source ('channels') and destination can be treated separately related to alignment, but
 * we don't take that way (though some gaining would be gathered if a frequent case). In our
 * environment is pretty likely dest will be 32-byte aligned. We define a fallback function
 * with little or no optimization in case of misalignment, and we use it in case dest is not
 * 32-byte aligned (dropping some edge cases which could be partially accelerated). We call
 * misaligned from this point on failing to be 32-byte aligned.
 *
 * Source alignment deserves some more attention:
 *   - if some input pointers are misaligned AND alignment is NOT THE VERY SAME FOR ALL
 *     (i.e: all are misaligned and all in the same amount), fallback to general, suboptimal
 *     implementation.
 *
 *   - if ALL input pointers are misaligned by the same amount:
 *       * if they're 8- or 16-byte aligned, handle special case till 32-byte boundary, then
 *         use the accelerated function. Note this can be a frequent case.
 *       * otherwise fall back to general case.
 *
 *   - if NONE input pointer is misaligned, proceed with the accelerated case.
 *
 * Considering this, the general case becomes:
 * Analyze pointers' alignment, then:
 *   - if some misalignment, fall back to general funtion.
 *   - if all input pointers 8- or 16-byte aligned, pack 8 first bytes and check again;
 *     follow till input pointers become 32-byte aligned
 *   - process bulk data 32-byte aligned and num_samples multiple of 32.
 *   - process the trailing data, which is 32-byte aligned but number of samples is less than 32.
 */
int channels_to_interleaved_8b(unsigned char *dst,
                               unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
                               int num_pixels)
{
    // We can user ARGB designation without loss of generality, but bear in mind order matters

    return channels_ileaved_d32_s32_n32m_8b_intrinsics(dst, a, r, g, b, num_pixels);
}
