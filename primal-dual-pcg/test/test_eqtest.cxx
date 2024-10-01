#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include "pkg/eqtest.hpp"
#include "test/fixtures.cxx"

class EqTestTests : public NetworkTest { };

TEST_F(EqTestTests, SizeReduction) {
  const uint32_t length = 128;
  const int threshold = 5;
  const int tests = 4;

  auto [srots, rrots] = ROT::mocked(EqTest::numOTs(length, threshold, tests));

  auto [sender, receiver] = this->launch(
    [&](Channel channel) -> EqTestSender {
      EqTestSender sender(length, threshold, tests, channel, srots);
      sender.sizeReduction(length);
      return sender;
    },
    [&](Channel channel) -> EqTestReceiver {
      EqTestReceiver receiver(length, threshold, tests, channel, rrots);
      receiver.sizeReduction(length);
      return receiver;
    }
  );

  for (size_t t = 0; t < tests; t++) {
    for (size_t i = 0; i < length; i++) {
      EXPECT_EQ(
          (sender.abi[t][0][i] + receiver.abi[t][0][i]) % (length + 1),
          sender.rsi[t][0][i] ^ receiver.rsi[t][0][i]
      );
    }
  }
}

TEST_F(EqTestTests, ProductSharing) {
  const uint32_t length = 128;
  const int threshold = 5;
  const int tests = 5;

  auto [srots, rrots] = ROT::mocked(EqTest::numOTs(length, threshold, tests));

  auto [sender, receiver] = this->launch(
    [&](Channel channel) -> EqTestSender {
      EqTestSender sender(length, threshold, tests, channel, srots);
      sender.productSharing();
      return sender;
    },
    [&](Channel channel) -> EqTestReceiver {
      EqTestReceiver receiver(length, threshold, tests, channel, rrots);
      receiver.productSharing();
      return receiver;
    }
  );

  for (size_t t = 0; t < tests; t++) {
    EXPECT_EQ(receiver.ab[t] ^ sender.ab[t], receiver.rs[t] & sender.rs[t]);
  }
}

TEST_F(EqTestTests, EqTestSingleTrue) {
  const uint32_t length = 8;
  const int threshold = 3;
  const int tests = 1;
  const uint32_t value = 2404;

  auto [srots, rrots] = ROT::mocked(EqTest::numOTs(length, threshold, tests));

  auto results = this->launch(
    [&](Channel channel) -> BitString {
      EqTestSender sender(length, threshold, tests, channel, srots);
      return sender.run(std::vector<uint32_t>({value}));
    },
    [&](Channel channel) -> BitString {
      EqTestReceiver receiver(length, threshold, tests, channel, rrots);
      return receiver.run(std::vector<uint32_t>({value}));
    }
  );
  BitString sout, rout;
  std::tie(sout, rout) = results;

  bool out = sout[0] ^ rout[0];
  EXPECT_TRUE(out);
}

TEST_F(EqTestTests, EqTestSingleFalse) {
  const uint32_t length = 8;
  const int threshold = 3;
  const int tests = 1;

  auto [srots, rrots] = ROT::mocked(EqTest::numOTs(length, threshold, tests));

  auto results = this->launch(
    [&](Channel channel) -> BitString {
      EqTestSender sender(length, threshold, tests, channel, srots);
      return sender.run(std::vector<uint32_t>({2404}));
    },
    [&](Channel channel) -> BitString {
      EqTestReceiver receiver(length, threshold, tests, channel, rrots);
      return receiver.run(std::vector<uint32_t>({593}));
    }
  );
  BitString sout, rout;
  std::tie(sout, rout) = results;

  bool out = sout[0] ^ rout[0];
  EXPECT_FALSE(out);
}

TEST_F(EqTestTests, EqTestBatch) {
  const uint32_t length = 8;
  const int threshold = 3;

  const std::vector<uint32_t> slist({1294, 451, 5942, 925, 1612, 2969, 2574, 252});
  const std::vector<uint32_t> rlist({1294, 294, 5942, 924, 1612, 2969, 226, 2562});

  const int tests = slist.size();

  auto [srots, rrots] = ROT::mocked(EqTest::numOTs(length, threshold, tests));

  auto results = this->launch(
    [&](Channel channel) -> BitString {
      EqTestSender sender(length, threshold, tests, channel, srots);
      return sender.run(slist);
    },
    [&](Channel channel) -> BitString {
      EqTestReceiver receiver(length, threshold, tests, channel, rrots);
      return receiver.run(rlist);
    }
  );

  BitString out = results.first ^ results.second;

  ASSERT_EQ(out.size(), slist.size());
  for (size_t i = 0; i < slist.size(); i++) {
    if (slist[i] == rlist[i]) {
      EXPECT_TRUE(out[i]);
    } else {
      EXPECT_FALSE(out[i]);
    }
  }
}

TEST_F(EqTestTests, EqTestNumOTs) {
  const uint32_t length = 8;
  const int threshold = 3;

  const std::vector<uint32_t> slist({1294, 451, 5942, 925, 1612, 2969, 2574, 252});
  const std::vector<uint32_t> rlist({1294, 294, 5942, 924, 1612, 2969, 226, 2562});

  const int tests = slist.size();

  auto [srots, rrots] = ROT::mocked(EqTest::numOTs(length, threshold, tests));

  uint32_t sbefore = srots.remaining();
  uint32_t rbefore = rrots.remaining();

  auto results = this->launch(
    [&](Channel channel) -> uint32_t {
      EqTestSender sender(length, threshold, tests, channel, srots);
      sender.run(slist);
      return 0;
    },
    [&](Channel channel) -> uint32_t {
      EqTestReceiver receiver(length, threshold, tests, channel, rrots);
      receiver.run(rlist);
      return 0;
    }
  );

  uint32_t safter = srots.remaining();
  uint32_t rafter = rrots.remaining();

  uint32_t actual = EqTest::numOTs(length, threshold, tests);

  EXPECT_EQ(sbefore - safter, actual);
  EXPECT_EQ(rbefore - rafter, actual);
}
