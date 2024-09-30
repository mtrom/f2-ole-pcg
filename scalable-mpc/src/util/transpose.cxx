#include <algorithm>
#include <cassert>
#include <emmintrin.h>
#include <stdexcept>
#include <vector>

#include "pkg/pprf.hpp"
#include "util/bitstring.hpp"
#include "util/concurrency.hpp"
#include "util/params.hpp"
#include "util/timer.hpp"

// taken from
// https://mischasan.wordpress.com/2011/10/03/the-full-sse2-bit-matrix-transpose-routine/
void sse_trans(uint8_t const *inp, uint8_t *out, int nrows, int ncols) {
#   define INP(x,y) inp[(x)*ncols/8 + (y)/8]
#   define OUT(x,y) out[(y)*nrows/8 + (x)/8]
  int rr, cc, i, h;
  union { __m128i x; uint8_t b[16]; } tmp;
  assert(nrows % 8 == 0 && ncols % 8 == 0);

  // Do the main body in 16x8 blocks:
  for (rr = 0; rr <= nrows - 16; rr += 16) {
    for (cc = 0; cc < ncols; cc += 8) {
      for (i = 0; i < 16; ++i) {
        tmp.b[i] = INP(rr + i, cc);
      }
      for (i = 8; --i >= 0; tmp.x = _mm_slli_epi64(tmp.x, 1)) {
        *(uint16_t*)&OUT(rr,cc+i)= _mm_movemask_epi8(tmp.x);
      }
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

void transpose(
  std::vector<BitString*>& input, std::vector<uint8_t*>& output, size_t nrows, size_t ncols
) {
  std::vector<uint8_t> inp_bytes(nrows * (ncols / 8));

  size_t nbytes = ((ncols + 7) / 8);
  for (int x = 0; x < nrows; x++) {
    uint8_t* src = input[x]->data();
    std::copy(src, src + nbytes, inp_bytes.begin() + (x * nbytes));
    input[x]->clear();
  }

  std::vector<uint8_t> out_bytes(ncols * (nrows / 8));
  sse_trans(inp_bytes.data(), out_bytes.data(), nrows, ncols);
  inp_bytes.clear();

  nbytes = ((nrows + 7) / 8);
  uint8_t* iter = out_bytes.data();
  for (int i = 0; i < ncols; i++) {
    std::copy(iter, iter + nbytes, output[i]);
    iter += nbytes;
  }
}

std::vector<BitString> transpose(std::vector<PPRF>& pprfs, const PCGParams& params) {
  Timer timer;
  // how many chunks each thread will process
  const size_t CHUNKS = 4;

  // the size of each chunk
  const size_t CHUNK_SIZE = params.dual.N() / (THREAD_COUNT * CHUNKS);
  std::vector<std::vector<BitString*>> in_ptrs(THREAD_COUNT * CHUNKS);

  timer.start("[transpose] prepare inputs");
  size_t n = 0;
  for (const PPRF& pprf : pprfs) {
    auto image = pprf.getImage();
    for (size_t i = 0; i < pprf.domain(); i++) {
      BitString* ptr = &((*image)[i]);
      in_ptrs[n / CHUNK_SIZE].push_back(ptr);
      n++;
      if (n >= params.dual.N()) { break; }
    }
  }
  timer.stop();

  timer.start("[transpose] prepare outputs");
  std::vector<BitString> output(params.primal.k, BitString(params.dual.N()));
  timer.stop();

  timer.start("[transpose] actual transpose");
  MULTI_TASK([&](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      std::vector<uint8_t*> out_ptrs;
      for (BitString& row : output) {
        out_ptrs.push_back(row.data() + (i * ((CHUNK_SIZE + 7) / 8)));
      }
      transpose(in_ptrs[i], out_ptrs, CHUNK_SIZE, params.primal.k);
      out_ptrs.clear();
    }
  }, THREAD_COUNT * CHUNKS);
  timer.stop();

  // free up processed memory
  timer.start("[transpose] memory clean up");
  for (PPRF& pprf : pprfs) { pprf.clear(); }
  timer.stop();

  return output;
}
