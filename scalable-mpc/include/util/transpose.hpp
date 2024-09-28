#include <assert.h>
#include <emmintrin.h>
#include <stdint.h>

void sse_trans(uint8_t** inp, uint8_t *out, int nrows, int ncols) {
#   define INP(x,y) inp[x][(y)/8]
#   define OUT(x,y) out[(y)*nrows/8 + (x)/8]
    int rr, cc, i, h;
    union { __m128i x; uint8_t b[16]; } tmp;
    assert(nrows % 8 == 0 && ncols % 8 == 0);

    // Do the main body in 16x8 blocks:
    for (rr = 0; rr <= nrows - 16; rr += 16) {
        for (cc = 0; cc < ncols; cc += 8) {
            for (i = 0; i < 16; ++i)
                tmp.b[i] = INP(rr + i, cc);
            for (i = 8; --i >= 0; tmp.x = _mm_slli_epi64(tmp.x, 1))
                *(uint16_t*)&OUT(rr,cc+i)= _mm_movemask_epi8(tmp.x);
        }
    }
    if (rr == nrows) return;

    // The remainder is a block of 8x(16n+8) bits (n may be 0).
    //  Do a PAIR of 8x8 blocks in each step:
    for (cc = 0; cc <= ncols - 16; cc += 16) {
        for (i = 0; i < 8; ++i) {
            tmp.b[i] = h = *(uint16_t const*)&INP(rr + i, cc);
            tmp.b[i + 8] = h >> 8;
        }
        for (i = 8; --i >= 0; tmp.x = _mm_slli_epi64(tmp.x, 1)) {
            OUT(rr, cc + i) = h = _mm_movemask_epi8(tmp.x);
            OUT(rr, cc + i + 8) = h >> 8;
        }
    }
    if (cc == ncols) return;

    //  Do the remaining 8x8 block:
    for (i = 0; i < 8; ++i)
        tmp.b[i] = INP(rr + i, cc);
    for (i = 8; --i >= 0; tmp.x = _mm_slli_epi64(tmp.x, 1))
        OUT(rr, cc + i) = _mm_movemask_epi8(tmp.x);
}
