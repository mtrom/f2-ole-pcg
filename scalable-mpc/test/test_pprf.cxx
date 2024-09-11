#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <thread>

#include "pkg/pprf.hpp"
#include "./fixtures.cxx"

#include "util/defines.hpp"
#include "util/random.hpp"


class PPRFTests : public NetworkTest { };

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

TEST_F(PPRFTests, Sample) {
  const size_t N = 16;
  std::vector<PPRF> pprfs = PPRF::sample(N, LAMBDA, LAMBDA, LAMBDA);

  for (size_t i = 0; i < LAMBDA; i++) {
    for (size_t j = 1; j < N; j++) {
      // crude way to check that it isn't just N copies of the same pprf
      EXPECT_NE(pprfs[j - 1](i), pprfs[j](i));
    }
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

TEST_F(PPRFTests, CompressSameOutput) {
  BitString key = BitString::sample(LAMBDA);

  PPRF a(key, LAMBDA, LAMBDA);
  PPRF b(key, LAMBDA, LAMBDA);

  a.compress();

  for (size_t i = 0; i < LAMBDA; i++) {
    EXPECT_EQ(a(i), b(i));
  }
}

TEST_F(PPRFTests, ConstructPunctured) {
  const size_t depth = 3;
  const size_t x = 5;
  std::vector<BitString> keys;
  for (size_t i = 0; i <= depth; i++) {
    keys.push_back(BitString::sample(LAMBDA));
  }
  PPRF pprf(keys, x, LAMBDA);

  // just ensure all the outputs are non-empty
  for (size_t i = 0; i < (1 << depth); i++) {
    EXPECT_EQ(pprf(i).size(), LAMBDA);
  }
}

TEST_F(PPRFTests, EvalSamePuncturedKeySameOutput) {
  const size_t depth = 3;
  const size_t x = 5;
  std::vector<BitString> keys;
  for (size_t i = 0; i <= depth; i++) {
    keys.push_back(BitString::sample(LAMBDA));
  }

  PPRF a(keys, x, LAMBDA);
  PPRF b(keys, x, LAMBDA);

  for (size_t i = 0; i < (1 << depth); i++) {
    EXPECT_EQ(a(i), b(i));
  }
}

TEST_F(PPRFTests, CompressedAndPuncturedSameOutput) {
  const size_t depth = 3;
  const size_t x = 5;
  std::vector<BitString> keys;
  for (size_t i = 0; i <= depth; i++) {
    keys.push_back(BitString::sample(LAMBDA));
  }

  PPRF a(keys, x, LAMBDA);
  PPRF b(keys, x, LAMBDA);

  a.compress();

  for (size_t i = 0; i < (1 << depth); i++) {
    EXPECT_EQ(a(i), b(i));
  }
}

TEST_F(PPRFTests, Image) {
  BitString key = BitString::sample(LAMBDA);

  PPRF pprf(key, 1, LAMBDA);
  BitString image = pprf.image();

  for (size_t i = 0; i < LAMBDA; i++) {
    EXPECT_EQ(pprf(i)[0], image[i]);
  }
}

TEST_F(PPRFTests, SendAndReceive) {
  const size_t x = 3;

  BitString payload = BitString::sample(LAMBDA);
  PPRF sender(BitString::sample(LAMBDA), LAMBDA, LAMBDA);

  auto results = this->launch(
    [&]() -> bool {
      PPRF::send(sender, payload, this->sch, this->srots);
      return true;
    },
    [&]() -> PPRF {
      return PPRF::receive(x, LAMBDA, LAMBDA, LAMBDA, this->rch, this->rrots);
    }
  );
  PPRF receiver = results.second;

  ASSERT_EQ(receiver.domain(), sender.domain());

  for (size_t i = 0; i < LAMBDA; i++) {
    if (i != x) {
      EXPECT_EQ(sender(i), receiver(i));
    } else {
      EXPECT_EQ(sender(i) ^ receiver(i), payload);
    }
  }
}

TEST_F(PPRFTests, SendAndReceiveShrinked) {
  const size_t x = 3;
  const size_t outsize = LAMBDA / 2;

  BitString payload = BitString::sample(outsize);
  PPRF sender(BitString::sample(LAMBDA), outsize, LAMBDA);

  auto results = this->launch(
    [&]() -> bool {
      PPRF::send(sender, payload, this->sch, this->srots);
      return true;
    },
    [&]() -> PPRF {
      return PPRF::receive(x, LAMBDA, outsize, LAMBDA, this->rch, this->rrots);
    }
  );
  PPRF receiver = results.second;

  ASSERT_EQ(receiver.domain(), sender.domain());

  for (size_t i = 0; i < LAMBDA; i++) {
    if (i != x) {
      EXPECT_EQ(sender(i), receiver(i));
    } else {
      EXPECT_EQ(sender(i) ^ receiver(i), payload);
    }
  }
}

TEST_F(PPRFTests, SendAndReceiveExpanded) {
  const size_t x = 3;
  const size_t outsize = LAMBDA * 2;

  BitString payload = BitString::sample(outsize);
  PPRF sender(BitString::sample(LAMBDA), outsize, LAMBDA);

  auto results = this->launch(
    [&]() -> bool {
      PPRF::send(sender, payload, this->sch, this->srots);
      return true;
    },
    [&]() -> PPRF {
      return PPRF::receive(x, LAMBDA, outsize, LAMBDA, this->rch, this->rrots);
    }
  );
  PPRF receiver = results.second;

  ASSERT_EQ(receiver.domain(), sender.domain());

  for (size_t i = 0; i < LAMBDA; i++) {
    if (i != x) {
      EXPECT_EQ(sender(i), receiver(i));
    } else {
      EXPECT_EQ(sender(i) ^ receiver(i), payload);
    }
  }
}

TEST_F(PPRFTests, SendAndReceiveDPF) {
  const size_t x = 3;
  const size_t outsize = 1;

  shared_ptr<CommParty> sch = this->sch;
  RandomOTSender srots = this->srots;

  BitString payload = BitString::fromUInt(1, 1);
  PPRF sender(BitString::sample(LAMBDA), outsize, LAMBDA);

  auto results = this->launch(
    [&]() -> bool {
      PPRF::send(sender, payload, this->sch, this->srots);
      return true;
    },
    [&]() -> PPRF {
      return PPRF::receive(x, LAMBDA, outsize, LAMBDA, this->rch, this->rrots);
    }
  );
  PPRF receiver = results.second;

  ASSERT_EQ(receiver.domain(), sender.domain());

  for (size_t i = 0; i < LAMBDA; i++) {
    BitString share = sender(i) ^ receiver(i);
    ASSERT_EQ(share.size(), 1);
    EXPECT_EQ(share[0], i == x);
  }
}

TEST_F(PPRFTests, SendAndReceiveBatched) {
  const size_t batchsize = LAMBDA;
  const size_t outsize = LAMBDA;

  std::vector<uint32_t> points = sampleVector(batchsize, LAMBDA);
  std::vector<PPRF> sender = PPRF::sample(batchsize, LAMBDA, outsize, LAMBDA);
  std::vector<BitString> payloads;
  for (size_t i = 0; i < batchsize; i++) {
    payloads.push_back(BitString::sample(outsize));
  }

  auto results = this->launch(
    [&]() -> bool {
      PPRF::send(sender, payloads, this->sch, this->srots);
      return true;
    },
    [&]() -> std::vector<PPRF> {
      return PPRF::receive(
          points, LAMBDA, outsize, LAMBDA, this->rch, this->rrots
      );

    }
  );
  std::vector<PPRF> receiver = results.second;

  for (size_t i = 0; i < batchsize; i++) {
    ASSERT_EQ(receiver[i].domain(), sender[i].domain());
    for (size_t x = 0; x < LAMBDA; x++) {
      if (x != points[i]) {
        EXPECT_EQ(sender[i](x), receiver[i](x));
      } else {
        EXPECT_EQ(sender[i](x) ^ receiver[i](x), payloads[i]);
      }
    }
  }
}

TEST_F(PPRFTests, SendAndReceiveBatchedSinglePayload) {
  const size_t batchsize = LAMBDA;
  const size_t outsize = LAMBDA;

  std::vector<uint32_t> points = sampleVector(batchsize, LAMBDA);
  std::vector<PPRF> sender = PPRF::sample(batchsize, LAMBDA, outsize, LAMBDA);
  BitString payload = BitString::sample(outsize);

  auto results = this->launch(
    [&]() -> bool {
      PPRF::send(sender, payload, this->sch, this->srots);
      return true;
    },
    [&]() -> std::vector<PPRF> {
      return PPRF::receive(
          points, LAMBDA, outsize, LAMBDA, this->rch, this->rrots
      );

    }
  );
  std::vector<PPRF> receiver = results.second;

  for (size_t i = 0; i < batchsize; i++) {
    ASSERT_EQ(receiver[i].domain(), sender[i].domain());
    for (size_t x = 0; x < LAMBDA; x++) {
      if (x != points[i]) {
        EXPECT_EQ(sender[i](x), receiver[i](x));
      } else {
        EXPECT_EQ(sender[i](x) ^ receiver[i](x), payload);
      }
    }
  }
}

TEST_F(PPRFTests, SendAndReceiveBatchedDPF) {
  const size_t batchsize = LAMBDA;
  const size_t outsize = 1;

  std::vector<uint32_t> points = sampleVector(batchsize, LAMBDA);
  BitString payloads = BitString::sample(batchsize);
  std::vector<PPRF> sender = PPRF::sample(batchsize, LAMBDA, outsize, LAMBDA);

  auto results = this->launch(
    [&]() -> bool {
      PPRF::sendDPFs(sender, payloads, this->sch, this->srots);
      return true;
    },
    [&]() -> std::vector<PPRF> {
      return PPRF::receive(
          points, LAMBDA, outsize, LAMBDA, this->rch, this->rrots
      );

    }
  );
  std::vector<PPRF> receiver = results.second;

  for (size_t i = 0; i < batchsize; i++) {
    ASSERT_EQ(receiver[i].domain(), sender[i].domain());
    for (size_t x = 0; x < LAMBDA; x++) {
      if (x != points[i]) {
        EXPECT_EQ(sender[i](x), receiver[i](x));
      } else {
        EXPECT_EQ((sender[i](x) ^ receiver[i](x))[0], payloads[i]);
      }
    }
  }
}
