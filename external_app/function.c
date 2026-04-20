#include <stdint.h>
#include <stddef.h>

__attribute__((used, visibility("default")))
float calculate_sqrt(float square, uint32_t* iters) {

    #define MINDIFF 1e-12

    if ( iters == NULL ) { return 0; }

    double root=square/3, last, diff=1;

    if (square <= 0) return 0;
    do {
        last = root;
        root = (root + square / root) / 2;
        diff = root - last;
        *iters++;
    } while (diff > MINDIFF || diff < -MINDIFF);
    
    return root;

}