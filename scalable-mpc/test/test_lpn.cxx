#include <gtest/gtest.h>

// allows us to test protected fields
#define protected public

#include "pkg/lpn.hpp"

using namespace LPN;

const size_t N = (1 << 12);
const size_t k = (1 << 6);
const size_t t = (1 << 8);
const size_t l = (1 << 3);

TEST(LPNTests, PrimalMatrixDims) {
  PrimalParams params(N, k, t, l);
  BitString key = BitString::sample(128);

  PrimalMatrix A(key, params);
  size_t height, width;
  std::tie(height, width) = A.dim();
  EXPECT_EQ(height, N);
  EXPECT_EQ(width, k);
}

TEST(LPNTests, PrimalMatrixRowWeight) {
  PrimalParams params(N, k, t, l);
  BitString key = BitString::sample(128);

  PrimalMatrix A(key, params);

  // all rows should have hamming weight l
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(A[i].weight(), l);
  }
}

TEST(LPNTests, DualMatrixDims) {
  DualParams params(N, 4, 32);
  BitString key = BitString::sample(128);

  DualMatrix H(key, params);
  size_t height, width;
  std::tie(height, width) = H.dim();
  EXPECT_EQ(height, N);
  EXPECT_EQ(width, N * 4);
}

TEST(LPNTests, MatrixProduct) {
  size_t P_HEIGHT = 128;
  size_t P_WIDTH = 64;
  size_t P_SPARSITY = 16;
  size_t D_EXPANSION = 4;

  PrimalParams pparams(P_HEIGHT, P_WIDTH, P_HEIGHT / 2, P_SPARSITY);
  PrimalMatrix primal = PrimalMatrix::sample(pparams);

  DualParams dparams(P_WIDTH, D_EXPANSION, P_WIDTH / 2);
  DualMatrix dual = DualMatrix::sample(dparams);

  MatrixProduct product = primal * dual;

  // since both matrices are randomly generated, hard to test directly
  // instead, test that P(Dv) = (PD)v which is the property we need anyhow

  BitString vector = BitString::sample(dual.dim().second);

  // since DualMatrix doesn't implement vector multiplication...
  BitString Dv(dual.dim().first);
  for (size_t row = 0; row < dual.dim().first; row++) {
    for (size_t col = 0; col < dual.dim().second; col++) {
      if (dual[{row, col}] && vector[col]) { Dv[row] = !Dv[row]; }
    }
  }

  BitString expected = primal * Dv;

  BitString actual = primal.dim().first;
  for (size_t i = 0; i < product.dim().first; i++) {
    actual[i] = product[i] * vector;
  }

  ASSERT_EQ(expected, actual);
}

TEST(LPNTests, EncryptorVectorMult) {
  const uint32_t HEIGHT = 8;
  const uint32_t WIDTH  = 4;

  EncryptionMatrix matrix(HEIGHT, WIDTH);

  BitString vector("1011");

  (*matrix.rows)[0] = BitString("0000");
  (*matrix.rows)[1] = BitString("1000");
  (*matrix.rows)[2] = BitString("0100");
  (*matrix.rows)[3] = BitString("1100");
  (*matrix.rows)[4] = BitString("0010");
  (*matrix.rows)[5] = BitString("1010");
  (*matrix.rows)[6] = BitString("0110");
  (*matrix.rows)[7] = BitString("1110");

  BitString actual = matrix * vector;
  ASSERT_EQ(actual.size(), HEIGHT);

  // I just did this by hand
  ASSERT_EQ(actual.toString(), "01011010");
}
