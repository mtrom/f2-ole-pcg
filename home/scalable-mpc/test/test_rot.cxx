#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <thread>

#include "pkg/rot.hpp"
#include "./fixtures.cxx"

class ROTTests : public NetworkTest { };

TEST_F(ROTTests, GetPassByReference) {
  auto get = [](RandomOTSender rot) -> void { rot.get(); };
  size_t before = this->srots.remaining();
  get(this->srots);
  size_t after = this->srots.remaining();
  EXPECT_EQ(before - 1, after);

  auto reference = this->srots;
  reference.get();
  reference.get();
  EXPECT_EQ(reference.remaining(), this->srots.remaining());
}

TEST_F(ROTTests, Run) {
  size_t total = 128;

  RandomOTSender sender;
  RandomOTReceiver receiver;

  this->launch(
    [&]() -> bool {
      sender.run(total, sch, 3000);
      return true;
    },
    [&]() -> bool {
      receiver.run(total, this->rch, 3000);
      return true;
    }
  );

  while (sender.remaining() > 0) {
    bool b;
    BitString m0, m1, mb;

    std::tie(m0, m1) = sender.get();
    std::tie( b, mb) = receiver.get();

    if (b) { EXPECT_EQ(mb, m1); }
    else   { EXPECT_EQ(mb, m0); }

    EXPECT_NE(m0, m1);
  }
}

TEST_F(ROTTests, GetSizes) {
  size_t total = 12;

  RandomOTSender sender;
  RandomOTReceiver receiver;

  this->launch(
    [&]() -> bool {
      sender.run(total, sch, 3000);
      return true;
    },
    [&]() -> bool {
      receiver.run(total, this->rch, 3000);
      return true;
    }
  );

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

  RandomOTSender sender;
  RandomOTReceiver receiver;

  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < ots; i++) {
    messages.push_back(std::make_pair(BitString::sample(m_size), BitString::sample(m_size)));
  }
  BitString choices = BitString::sample(ots);

  auto results = this->launch(
    [&]() -> bool {
      sender.run(total, this->sch, 3000);
      sender.transfer(messages, this->sch);
      return true;
    },
    [&]() -> std::vector<BitString> {
      receiver.run(total, this->rch, 3000);
      return receiver.transfer(choices, m_size, this->rch);
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

  RandomOTSender sender;
  RandomOTReceiver receiver;

  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < sizes.size(); i++) {
    messages.push_back(std::make_pair(BitString::sample(sizes[i]), BitString::sample(sizes[i])));
  }
  BitString choices = BitString::sample(sizes.size());

  auto results = this->launch(
    [&]() -> bool {
      sender.run(total, this->sch, 3000);
      sender.transfer(messages, this->sch);
      return true;
    },
    [&]() -> std::vector<BitString> {
      receiver.run(total, this->rch, 3000);
      return receiver.transfer(choices, sizes, this->rch);
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

  RandomOTSender sender;
  RandomOTReceiver receiver;

  BitString m0 = BitString::sample(ots);
  BitString m1 = BitString::sample(ots);
  BitString choices = BitString::sample(ots);

  auto results = this->launch(
    [&]() -> bool {
      sender.run(total, this->sch, 3000);
      sender.transfer(m0, m1, this->sch);
      return true;
    },
    [&]() -> BitString {
      receiver.run(total, this->rch, 3000);
      return receiver.transfer(choices, this->rch);
    }
  );
  BitString mbs = results.second;

  for (size_t i = 0; i < ots; i++) {
    EXPECT_EQ(mbs[i], choices[i] ? m1[i] : m0[i]);
  }
}
