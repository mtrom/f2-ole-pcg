#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <thread>

#include "pkg/rot.hpp"
#include "test/fixtures.cxx"

class ROTTests : public NetworkTest { };

TEST_F(ROTTests, GetPassByReference) {
  auto get = [](ROT::Sender rot) -> void { rot.get(); };
  auto [srots, rrots] = ROT::mocked(8);
  size_t before = srots.remaining();
  get(srots);
  size_t after = srots.remaining();
  EXPECT_EQ(before - 1, after);

  auto reference = srots;
  reference.get();
  reference.get();
  EXPECT_EQ(reference.remaining(), srots.remaining());
}

TEST_F(ROTTests, GetSizes) {
  size_t total = 12;
  auto [sender, receiver] = ROT::mocked(total);

  // try get() on sizes {2^0, 2^1, ..., 2^12}
  for (size_t i = 0; i < total; i++) {
    bool b;
    BitString m0, m1, mb;

    std::tie(m0, m1) = sender.get(1 << i);
    std::tie( b, mb) = receiver.get(1 << i);

    if (b) { EXPECT_EQ(mb, m1); }
    else   { EXPECT_EQ(mb, m0); }
  }
}

TEST_F(ROTTests, TransferMultiBit) {
  size_t total = 128;
  size_t ots = 100;
  size_t m_size = 64;
  auto [sender, receiver] = ROT::mocked(total);

  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < ots; i++) {
    messages.push_back(std::make_pair(BitString::sample(m_size), BitString::sample(m_size)));
  }
  BitString choices = BitString::sample(ots);

  auto results = this->launch(
    [&](Channel channel) -> bool {
      sender.transfer(messages, channel);
      return true;
    },
    [&](Channel channel) -> std::vector<BitString> {
      return receiver.transfer(choices, m_size, channel);
    }
  );
  std::vector<BitString> mbs = results.second;

  for (size_t i = 0; i < ots; i++) {
    EXPECT_EQ(mbs[i], choices[i] ? messages[i].second : messages[i].first);
  }
}

TEST_F(ROTTests, TransferDifferingSizes) {
  size_t total = 128;
  std::vector<size_t> sizes({1, 2, 4, 8, 16, 32, 64, 128});
  auto [sender, receiver] = ROT::mocked(total);

  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < sizes.size(); i++) {
    messages.push_back(std::make_pair(BitString::sample(sizes[i]), BitString::sample(sizes[i])));
  }
  BitString choices = BitString::sample(sizes.size());

  auto results = this->launch(
    [&](Channel channel) -> bool {
      sender.transfer(messages, channel);
      return true;
    },
    [&](Channel channel) -> std::vector<BitString> {
      return receiver.transfer(choices, sizes, channel);
    }
  );
  std::vector<BitString> mbs = results.second;

  for (size_t i = 0; i < sizes.size(); i++) {
    EXPECT_EQ(mbs[i], choices[i] ? messages[i].second : messages[i].first);
  }
}


TEST_F(ROTTests, TransferSingleBit) {
  size_t total = 128;
  size_t ots = 100;
  auto [sender, receiver] = ROT::mocked(total);

  BitString m0 = BitString::sample(ots);
  BitString m1 = BitString::sample(ots);
  BitString choices = BitString::sample(ots);

  auto results = this->launch(
    [&](Channel channel) -> bool {
      sender.transfer(m0, m1, channel);
      return true;
    },
    [&](Channel channel) -> BitString {
      return receiver.transfer(choices, channel);
    }
  );
  BitString mbs = results.second;

  for (size_t i = 0; i < ots; i++) {
    EXPECT_EQ(mbs[i], choices[i] ? m1[i] : m0[i]);
  }
}
