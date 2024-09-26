#include <gtest/gtest.h>

#include <algorithm>
#include <boost/asio.hpp>
#include <thread>

#include "pkg/pprf.hpp"
#include "./fixtures.cxx"

#include "util/defines.hpp"
#include "util/random.hpp"


class PPRFTests : public NetworkTest { };
class DPFTests  : public NetworkTest { };

TEST_F(PPRFTests, ConstructorLambdaOutput) {
  BitString key = BitString::sample(LAMBDA);

  PPRF pprf(key, LAMBDA, LAMBDA);

  for (size_t i = 0; i < LAMBDA; i++) {
    EXPECT_EQ(LAMBDA, pprf(i).size());
  }
}

TEST_F(PPRFTests, ConstructorShrinkOutput) {
  BitString key = BitString::sample(LAMBDA);
  size_t outsize = 64;

  PPRF pprf(key, outsize, LAMBDA);

  for (size_t i = 0; i < LAMBDA; i++) {
    EXPECT_EQ(outsize, pprf(i).size());
  }
}

TEST_F(PPRFTests, ConstructorExpandOutput) {
  BitString key = BitString::sample(LAMBDA);
  size_t outsize = 256;

  PPRF pprf(key, outsize, LAMBDA);

  for (size_t i = 0; i < LAMBDA; i++) {
    EXPECT_EQ(outsize, pprf(i).size());
  }
}

TEST_F(PPRFTests, EvalSameKeySameOutput) {
  BitString key = BitString::sample(LAMBDA);

  PPRF a(key, LAMBDA, LAMBDA);
  PPRF b(key, LAMBDA, LAMBDA);

  for (size_t i = 0; i < LAMBDA; i++) {
    EXPECT_EQ(a(i), b(i));
  }
}

TEST_F(PPRFTests, ConstructPunctured) {
  const size_t depth = 3;
  const size_t x = 5;
  std::vector<BitString> keys;
  for (size_t i = 0; i < depth + 1; i++) {
    keys.push_back(BitString::sample(LAMBDA));
  }

  PPRF pprf(keys, x, LAMBDA, 1 << depth);

  pprf.expand();

  // just ensure all the outputs are non-empty
  for (size_t i = 0; i < (1 << depth); i++) {
    EXPECT_EQ(pprf(i).size(), LAMBDA);
  }
}

TEST_F(PPRFTests, EvalSamePuncturedKeySameOutput) {
  const size_t depth = 3;
  const size_t x = 5;
  std::vector<BitString> keys;
  for (size_t i = 0; i < depth + 1; i++) {
    keys.push_back(BitString::sample(LAMBDA));
  }

  PPRF a(keys, x, LAMBDA, 1 << depth);
  PPRF b(keys, x, LAMBDA, 1 << depth);

  a.expand();
  b.expand();

  for (size_t i = 0; i < (1 << depth); i++) {
    EXPECT_EQ(a(i), b(i));
  }
}

TEST_F(PPRFTests, SendAndReceive) {
  const size_t batchsize = 64;
  const size_t outsize = LAMBDA;
  const size_t domainsize = 512;

  std::vector<uint32_t> points = sampleDistinct(batchsize, domainsize);
  std::vector<PPRF> sender = PPRF::sample(batchsize, LAMBDA, outsize, domainsize);
  BitString payload = BitString::sample(outsize);

  auto results = this->launch(
    [&]() -> bool {
      PPRF::send(sender, payload, this->sch, this->srots);
      return true;
    },
    [&]() -> std::vector<PPRF> {
      return PPRF::receive(
        points, LAMBDA, outsize, domainsize, this->rch, this->rrots
      );

    }
  );
  std::vector<PPRF> receiver = results.second;

  for (size_t i = 0; i < sender.size(); i++) {
    ASSERT_EQ(receiver[i].domain(), sender[i].domain());
    receiver[i].expand();
    for (size_t x = 0; x < domainsize; x++) {
      if (x == points[i]) {
        EXPECT_EQ(sender[i](x) ^ receiver[i](x), payload);
      } else {
        EXPECT_EQ(sender[i](x), receiver[i](x));
      }
    }
  }
}

TEST_F(DPFTests, SameKeySameImage) {
  BitString key = BitString::sample(LAMBDA);

  DPF a(key, LAMBDA);
  DPF b(key, LAMBDA);
  EXPECT_EQ(a.image(), b.image());
}

TEST_F(DPFTests, SendAndReceive) {
  const size_t batchsize = LAMBDA;

  std::vector<uint32_t> points = sampleVector(batchsize, LAMBDA);
  BitString payloads = BitString::sample(batchsize);
  std::vector<DPF> sender = DPF::sample(batchsize, LAMBDA, LAMBDA);

  auto results = this->launch(
    [&]() -> bool {
      DPF::send(sender, payloads, this->sch, this->srots);
      return true;
    },
    [&]() -> std::vector<DPF> {
      return DPF::receive(
          points, LAMBDA, LAMBDA, this->rch, this->rrots
      );
    }
  );
  std::vector<DPF> receiver = results.second;

  for (size_t i = 0; i < batchsize; i++) {
    ASSERT_EQ(receiver[i].domain(), sender[i].domain());
    receiver[i].expand();
    BitString actual = sender[i].image() ^ receiver[i].image();
    BitString expected(LAMBDA);
    expected[points[i]] = payloads[i];
    if (expected != actual) {
      std::cout << points[i] << std::endl;
    }
    EXPECT_EQ(expected, actual);
  }
}
