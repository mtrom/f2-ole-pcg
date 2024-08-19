#include <gtest/gtest.h>

#include "util/random.hpp"
#include "util/defines.hpp"

TEST(TestRandom, SampleLessThan) {
  uint32_t a = sampleLessThan(123456);
  uint32_t b = sampleLessThan(123456);

  // its hard to check "randomness" so just make sure they are
  //  in the range and not the same
  EXPECT_LT(a, 123456);
  EXPECT_LT(b, 123456);
  EXPECT_NE(a, b);
}

TEST(TestRandom, BitSampler) {
  BitSampler sampler;

  // just ensure this doesn't crash even after more than LAMBDA calls
  for (size_t i = 0; i < 4 * LAMBDA; i++) {
    bool bit = sampler.get();
  }
}

TEST(PRFTests, SingleEvalWithinMax) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1024;
  uint32_t max = 1 << 24;

  PRF<uint32_t> prf(key);
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_LT(prf(in, max), max);
  }
}

TEST(PRFTests, SingleEvalSameInputSameOutput) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1024;
  uint32_t max = 1 << 24;

  PRF<uint32_t> prf(key);
  std::vector<uint32_t> outputs;
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_EQ(prf(in, max), prf(in, max));
    outputs.push_back(prf(in, max));
  }

  // also check after going through all inputs
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_EQ(prf(in, max), outputs[in]);
  }
}

TEST(PRFTests, SingleEvalDiffInputDiffOutput) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1 << 10;
  uint32_t max = 1 << 30;

  std::set<uint32_t> image;

  PRF<uint32_t> prf(key);
  for (size_t in = 0; in < TESTS; in++) {
    uint32_t out = prf(in, max);
    EXPECT_TRUE(image.find(out) == image.end());
    image.insert(out);
  }
}

// this is a silly test
TEST(PRFTests, SingleEvalSimilarInputsVeryDifferentOutputs) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t in = sampleLessThan(1 << 31);
  uint32_t TESTS = 1 << 10;
  uint32_t max = 1 << 31;

  PRF<uint32_t> prf(key);
  BitString diffs = BitString::fromUInt(prf(in, max)) ^ BitString::fromUInt(prf(in + 1, max));

  // check that the outputs are different in at least four bits
  // of course this will happen sometimes, but should hopefully be negligible
  EXPECT_GT(diffs.weight(), 4);
}

TEST(PRFTests, PairEvalWithinMax) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1024;
  uint32_t max = 1 << 24;

  PRF<uint32_t> prf(key);
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_LT(prf(std::make_pair(in, in + 1), max), max);
  }
}

TEST(PRFTests, PairEvalSameInputSameOutput) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1024;
  uint32_t max = 1 << 24;

  PRF<uint32_t> prf(key);
  std::vector<uint32_t> outputs;
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_EQ(prf(std::make_pair(in, in), max), prf(std::make_pair(in, in), max));
    outputs.push_back(prf(std::make_pair(in, in), max));
  }

  // also check after going through all inputs
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_EQ(prf(std::make_pair(in, in), max), outputs[in]);
  }
}

TEST(PRFTests, PairEvalDiffInputDiffOutput) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1 << 10;
  uint32_t max = 1 << 30;

  std::set<uint32_t> image;

  PRF<uint32_t> prf(key);
  for (size_t in = 0; in < TESTS; in++) {
    uint32_t out = prf(std::make_pair(in, in), max);
    EXPECT_TRUE(image.find(out) == image.end());
    image.insert(out);
  }
}

// this is a silly test
TEST(PRFTests, PairEvalSimilarInputsVeryDifferentOutputs) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t in = sampleLessThan(1 << 31);
  uint32_t TESTS = 1 << 10;
  uint32_t max = 1 << 31;

  PRF<uint32_t> prf(key);
  BitString diffs = (
    BitString::fromUInt(prf(std::make_pair(in, in), max))
    ^ BitString::fromUInt(prf(std::make_pair(in, in + 1), max))
  );

  // check that the outputs are different in at least four bits
  // of course this will happen sometimes, but should hopefully be negligible
  EXPECT_GT(diffs.weight(), 4);
}

TEST(PRFTests, BitStringSize) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1024;
  uint32_t bits = 1 << 10;

  PRF<BitString> prf(key);
  for (size_t i = 0; i < TESTS; i++) {
    EXPECT_EQ(prf(i, bits).size(), bits);
  }
}

TEST(PRFTests, BitStringEvalSameInputSameOutput) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1024;
  uint32_t bits = 1 << 10;

  PRF<BitString> prf(key);
  std::vector<BitString> outputs;
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_EQ(prf(in, bits), prf(in, bits));
    outputs.push_back(prf(in, bits));
  }

  // also check after going through all inputs
  for (size_t in = 0; in < TESTS; in++) {
    EXPECT_EQ(prf(in, bits), outputs[in]);
  }
}

TEST(PRFTests, BitStringEvalDiffInputDiffOutput) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t TESTS = 1 << 10;
  uint32_t bits = 1 << 10;

  std::vector<BitString> image;

  PRF<BitString> prf(key);
  for (size_t in = 0; in < TESTS; in++) {
    BitString out = prf(in, bits);
    for (size_t i = 0; i < image.size(); i++) {
      ASSERT_NE(image[i], out);
    }
    image.push_back(out);
  }
}

// this is a silly test
TEST(PRFTests, BitStringEvalSimilarInputsVeryDifferentOutputs) {
  BitString key = BitString::sample(LAMBDA);
  uint32_t in = sampleLessThan(1 << 31);
  uint32_t TESTS = 1 << 10;
  uint32_t bits = 1 << 10;

  PRF<BitString> prf(key);
  BitString diffs = prf(in, bits) ^ prf(in + 1, bits);

  // check that the outputs are different in at least four bits
  // of course this will happen sometimes, but should hopefully be negligible
  EXPECT_GT(diffs.weight(), 4);
}

TEST(GaussianSamplerTests, Constructor) {
  // just testing there is no exception
  GaussianSampler sampler = GaussianSampler::getInstance();
}

TEST(GaussianSamplerTests, Sample) {
  GaussianSampler sampler = GaussianSampler::getInstance();

  std::vector<uint32_t> obs((2 * sampler.tail()) + 1);

  for (size_t i = 0; i <= (1 << 16); i++) {
    int observation = sampler.get();
    obs[observation + sampler.tail()]++;
  }
  std::cout << std::endl;

  uint32_t max = *std::max_element(obs.begin(), obs.end());

  // it is hard to assert that the observed distribution is Gaussian, so instead the test
  //  will print a visualization of the distribution and we will rely on human eye test.
  for (int i = 0; i < obs.size(); i++) {
    int observation = i - static_cast<int>(sampler.tail());
    int result = static_cast<int>((static_cast<double>(obs[i]) / max) * 50);
    std::cout << (observation >= 0 ? "    " : "   ") << observation << "\t| ";
    std::cout << std::string(result, '#') << std::endl;
  }
}
