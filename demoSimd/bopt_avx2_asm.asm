; ----------------------------------------------------------------------------------------
; Some optimized functions using SIMD up to AVX2
; ----------------------------------------------------------------------------------------

; @brief channels to interleaved, no alignment at all, num_samples multiple of 32.
; @remark channels must be 8-bit per channel
; @remark num_pixels MUST BE multiple of 32
;
; extern int channels_ileaved_dmis_smis_n32m_8b_asm(
;               unsigned char *dst,
;               unsigned char *a, unsigned char *r, unsigned char *g, unsigned char *b,
;               int num_pixels);
;
; Packs 4 independent channels, say A,R,G,B to interleaved format ARGB.
;
; rdi, rsi, rdx, rcx, r8, r9
;
;	rdi:  unsigned char *dst
;	rsi:  unsigned char *a
;	rdx:  unsigned char *r
;	rcx:  unsigned char *g
;	r8 :  unsigned char *b
;	r9 :  int num_pixels
;	returns:  rax int num of pixels packed   

        global      channels_ileaved_dmis_smis_n32m_8b_asm

        section     .text

channels_ileaved_dmis_smis_n32m_8b_asm:

        mov         rax, rdi            	; copies 1st operand
        add         rax, rsi            	; adds 2nd operand
        ret                    			; returns sum in rax

;          section   .data
;message:  db        "Hello, World", 10      ; note the newline at the end

;{
;    const int pixels_per_iteration = sizeof( __m256i ) / 1;     // 1 byte per pixel on each channel
;    const int num_iterations = num_pixels / pixels_per_iteration;
;    assert( !(num_pixels % pixels_per_iteration) );

;    const __m256i *pr = (const __m256i *) r;
;    const __m256i *pg = (const __m256i *) g;
;    const __m256i *pb = (const __m256i *) b;
;    const __m256i *pa = (const __m256i *) a;

;    __m256i *pdst = (__m256i *) dst;
;    __m256i rs, gs, bs, as;                 // each one an AVX value of all R, all G, all B, all A
;    __m256i ar, gb;                         // pairs AR and GB
;    __m256i argbT0, argbT1;                 // tmps to shuffle bytes
;    __m256i argb0, argb1, argb2, argb3;     // 4*32 bytes interleaved result

;    if ( !a ) as = _mm256_set1_epi8( 0xFF );// set fixed alpha value, if applies
;    int num_pixels_copied = 0;
;    for(int i=0; i<num_iterations; i++) {
;        rs = _mm256_loadu_si256( pr );       // 32 bytes of R-channel
;        gs = _mm256_loadu_si256( pg );
;        bs = _mm256_loadu_si256( pb );
;        if ( a ) as = _mm256_loadu_si256( pa );

;        ar = _mm256_unpacklo_epi8( as, rs );    // low parts
;        gb = _mm256_unpacklo_epi8( gs, bs );

;        argbT0 = _mm256_unpacklo_epi16( ar, gb );
;        argbT1 = _mm256_unpackhi_epi16( ar, gb );
;        argb0  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 ); // take low  128 bits of each
;        argb2  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 ); // take high 128 bits of each

;        ar = _mm256_unpackhi_epi8( as, rs );   // high parts
;        gb = _mm256_unpackhi_epi8( gs, bs );

;        argbT0 = _mm256_unpacklo_epi16( ar, gb );
;        argbT1 = _mm256_unpackhi_epi16( ar, gb );
;        argb1  = _mm256_permute2x128_si256( argbT0, argbT1, 0x20 );
;        argb3  = _mm256_permute2x128_si256( argbT0, argbT1, 0x31 );

;        _mm256_storeu_si256( pdst++, argb0 );
;        _mm256_storeu_si256( pdst++, argb1 );
;        _mm256_storeu_si256( pdst++, argb2 );
;        _mm256_storeu_si256( pdst++, argb3 );

;        pr++; pg++; pb++;
;        pa++;       // this one ignored if fixed alpha, but faster than checking
;        num_pixels_copied += pixels_per_iteration;
;    }

;    return num_pixels_copied;
;}
