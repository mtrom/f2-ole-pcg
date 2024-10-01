#include <gtest/gtest.h>

// allows us to test private methods
#define protected public
#define private public

#include "ahe/ahe.hpp"
#include "pkg/eqtest.hpp"
#include "pkg/pcg.hpp"
#include "test/fixtures.cxx"

#include "util/defines.hpp"

class PCGTests : public NetworkTest { };

// insecure but small params to test with
PCGParams TEST_PARAMS(
  BitString::sample(LAMBDA), 1 << 16, 1 << 10, 1 << 8, 1 << 3,
  BitString::sample(LAMBDA), 1 << 2, 1 << 5
);

TEST_F(PCGTests, PCGRun) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 12, 1 << 7, 1 << 6, 1 << 3,
    BitString::sample(LAMBDA), 4, 7
  );

  PCG::Sender alice(params);
  PCG::Receiver bob(params);

  std::pair<size_t, size_t> nOTs = alice.numOTs();
  auto [alice_srots, bob_rrots] = ROT::mocked(nOTs.first);
  auto [bob_srots, alice_rrots] = ROT::mocked(nOTs.second);

  auto results = this->launch(
    [&](Channel channel) -> BitString {
      EC::Curve curve; // needed to initalize relic on this thread
      return alice.run(channel, alice_srots, alice_rrots);
    },
    [&](Channel channel) -> BitString {
      EC::Curve curve; // needed to initalize relic on this thread
      return bob.run(channel, bob_srots, bob_rrots);
    }
  );

  // 2-party correlation inputs
  BitString a = alice.inputs();
  BitString b = bob.inputs();

  // 2-party correlation outputs
  BitString c0 = results.first;
  BitString c1 = results.second;

  ASSERT_EQ(a & b, c0 ^ c1);
}

TEST_F(PCGTests, PCGNumOTs) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 12, 1 << 7, 1 << 6, 1 << 3,
    BitString::sample(LAMBDA), 4, 7
  );

  PCG::Sender alice(params);
  PCG::Receiver bob(params);

  std::pair<size_t, size_t> nOTs = alice.numOTs();
  auto [alice_srots, bob_rrots] = ROT::mocked(nOTs.first);
  auto [bob_srots, alice_rrots] = ROT::mocked(nOTs.second);

  auto results = this->launch(
    [&](Channel channel) -> BitString {
      EC::Curve curve; // needed to initalize relic on this thread
      return alice.run(channel, alice_srots, alice_rrots);
    },
    [&](Channel channel) -> BitString {
      EC::Curve curve; // needed to initalize relic on this thread
      return bob.run(channel, bob_srots, bob_rrots);
    }
  );

  EXPECT_EQ(0, alice_srots.remaining());
  EXPECT_EQ(0, alice_rrots.remaining());
  EXPECT_EQ(0, bob_srots.remaining());
  EXPECT_EQ(0, bob_rrots.remaining());
}
