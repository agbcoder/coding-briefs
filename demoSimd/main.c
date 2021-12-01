#include "bopt_avx2.h"

#include <stdlib.h>

void do_something() {
    const int align_forced = 32;
    const int num_pixels = 32*2;

    unsigned char *r, *g, *b, *a;
    unsigned char *img;

    r = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );
    g = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );
    b = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );
    a = (unsigned char *) aligned_alloc( align_forced, num_pixels * sizeof(unsigned char) );

    img = (unsigned char *) aligned_alloc( align_forced, num_pixels * 4*sizeof(unsigned char) );  // ARGB interleaved

    // slow but safe...
    for(unsigned int i = 0; i<num_pixels; i++) {
        r[i] = 0xE1 + i;
        g[i] = 0x80 + i;
        b[i] = 0x12 + i;
        a[i] = 0xFF;
    }

    channels_to_interleaved_8b(img, NULL, r, g, b, num_pixels);

    free(r); free(g); free(b); free(a);
    free(img);

    return;
}

int main(int argc, char **argv) {
    do_something();

    return 0;
}
