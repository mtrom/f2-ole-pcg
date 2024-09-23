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

TEST(LPNTests, PrimalMatrixAccess) {
  PrimalParams params(N, k, t, l);
  BitString key = BitString::sample(128);

  PrimalMatrix A(key, params);
  for (size_t i = 0; i < N; i++) {
    BitString row = A[i];
    for (size_t j = 0; j < k; j++) {
      bool expected = row[j];
      bool actual = A[{i, j}];
      ASSERT_EQ(expected, actual);
    }
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

TEST(LPNTests, PrimalDualMultAllZeros) {
  size_t P_HEIGHT = 8;
  size_t P_WIDTH = 4;
  size_t P_SPARSITY = 3;
  size_t D_EXPANSION = 2;

  PrimalParams pparams(P_HEIGHT, P_WIDTH, P_HEIGHT / 2, P_SPARSITY);
  PrimalMatrix primal = PrimalMatrix::sample(pparams);

  DualParams dparams(P_WIDTH, D_EXPANSION, P_WIDTH / 2);
  DualMatrix dual = DualMatrix::sample(dparams);

  dual.rows = std::make_shared<std::vector<BitString>>(std::vector<BitString>({
    BitString("00000000"),
    BitString("00000000"),
    BitString("00000000"),
    BitString("00000000"),
  }));

  MatrixProduct B = primal * dual;
  ASSERT_EQ(B.dim(), std::make_pair(P_HEIGHT, P_WIDTH * D_EXPANSION));
  for (size_t i = 0; i < B.dim().first; i++) {
    EXPECT_EQ(B[i].toString(), std::string(P_WIDTH * D_EXPANSION, '0'));
  }
}

TEST(LPNTests, PrimalDualMult) {
  size_t P_HEIGHT = 8;
  size_t P_WIDTH = 4;
  size_t P_SPARSITY = 2;
  size_t D_EXPANSION = 2;

  PrimalParams pparams(P_HEIGHT, P_WIDTH, P_HEIGHT / 2, P_SPARSITY);
  PrimalMatrix primal = PrimalMatrix::sample(pparams);

  primal.points = std::make_shared<std::vector<std::vector<uint32_t>>>(
    std::vector<std::vector<uint32_t>>({
      std::vector<uint32_t>({0, 1}),
      std::vector<uint32_t>({0, 2}),
      std::vector<uint32_t>({0, 3}),
      std::vector<uint32_t>({1, 2}),
      std::vector<uint32_t>({1, 3}),
      std::vector<uint32_t>({2, 3}),
      std::vector<uint32_t>({0, 1}),
      std::vector<uint32_t>({0, 2}),
    })
  );

  DualParams dparams(P_WIDTH, D_EXPANSION, P_WIDTH / 2);
  DualMatrix dual = DualMatrix::sample(dparams);

  dual.rows = std::make_shared<std::vector<BitString>>(std::vector<BitString>({
    BitString("01010101"),
    BitString("00110011"),
    BitString("00001111"),
    BitString("00000000"),
  }));

  MatrixProduct B = primal * dual;
  ASSERT_EQ(B.dim(), std::make_pair(P_HEIGHT, P_WIDTH * D_EXPANSION));

  // I just did these by hand
  EXPECT_EQ(B[0].toString(), "01100110");
  EXPECT_EQ(B[1].toString(), "01011010");
  EXPECT_EQ(B[2].toString(), "01010101");
  EXPECT_EQ(B[3].toString(), "00111100");
  EXPECT_EQ(B[4].toString(), "00110011");
  EXPECT_EQ(B[5].toString(), "00001111");
  EXPECT_EQ(B[6].toString(), "01100110");
  EXPECT_EQ(B[7].toString(), "01011010");
}

TEST(LPNTests, DenseVectorMult) {
  const uint32_t HEIGHT = 8;
  const uint32_t WIDTH  = 4;

  DenseMatrix matrix(HEIGHT, WIDTH);

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

TEST(LPNTests, SparseVectorMult) {
  const uint32_t HEIGHT = 8;
  const uint32_t WIDTH  = 4;

  SparseMatrix matrix(HEIGHT, WIDTH);

  BitString vector("1011");

  matrix.points = std::make_shared<std::vector<std::vector<uint32_t>>>(
    std::vector<std::vector<uint32_t>>({
      std::vector<uint32_t>({0, 1}),
      std::vector<uint32_t>({0, 2}),
      std::vector<uint32_t>({0, 3}),
      std::vector<uint32_t>({1, 2}),
      std::vector<uint32_t>({1, 3}),
      std::vector<uint32_t>({2, 3}),
      std::vector<uint32_t>({0, 1}),
      std::vector<uint32_t>({0, 2}),
    })
  );

  BitString actual = matrix * vector;
  ASSERT_EQ(actual.size(), HEIGHT);

  // I just did this by hand
  ASSERT_EQ(actual.toString(), "10011010");
}
