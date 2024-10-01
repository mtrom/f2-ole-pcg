#include <gtest/gtest.h>

#include <future>
#include <utility>
#include <boost/asio.hpp>
#include <libscapi/include/comm/Comm.hpp>

#include "pkg/pcg.hpp"
#include "pkg/rot.hpp"

#define TEST_BASE_PORT 3200
#define TEST_COMM_SLEEP 500
#define TEST_COMM_TIMEOUT 5000

using address = boost::asio::ip::address;

// test fixture to test methods over a channel
class NetworkTest : public testing::Test {
protected:
  // network information for a sender & receiver
  boost::asio::io_service sios;
  boost::asio::io_service rios;
  shared_ptr<CommParty> sch;
  shared_ptr<CommParty> rch;

  // mocked random ots
  RandomOTSender srots;
  RandomOTReceiver rrots;
  const size_t ROTS = 1 << 14;

  void SetUp() override {
    SocketPartyData sender(address::from_string("127.0.0.1"), TEST_BASE_PORT);
    SocketPartyData receiver(address::from_string("127.0.0.1"), TEST_BASE_PORT + 1);
    this->sch = make_shared<CommPartyTCPSynced>(this->sios, sender, receiver);
    this->rch = make_shared<CommPartyTCPSynced>(this->rios, receiver, sender);
    std::tie(this->srots, this->rrots) = mockRandomOT(ROTS);
  }

  // run two methods on two different threads and report the results
  template<typename Sender, typename Receiver>
  auto launch(Sender s, Receiver r) -> std::pair<decltype(s()), decltype(r())> {
    auto sfuture = std::async(std::launch::async, [this, s]() {
      this->sch->join(TEST_COMM_SLEEP, TEST_COMM_TIMEOUT);
      return s();
    });

    auto rfuture = std::async(std::launch::async, [this, r]() {
      this->rch->join(TEST_COMM_SLEEP, TEST_COMM_TIMEOUT);
      return r();
    });

    return {sfuture.get(), rfuture.get()};
  }
};

// test fixture to test methods over a channel
class MultiNetworkTest : public testing::Test {
protected:
  // number of parties required
  const int n;

  // network information multiple parties
  std::vector<boost::asio::io_service> ios;
  std::vector<Channel> incoming;
  std::vector<Channel> outgoing;

  MultiNetworkTest(int n = 2) : n(n), ios(n), outgoing(n - 1), incoming(n - 1) { }

  void SetUp() override {
    // set up channel between the first party and each other party
    for (int i = 0; i < n - 1; i++) {
      SocketPartyData in(address::from_string("127.0.0.1"), TEST_BASE_PORT + i);
      SocketPartyData out(address::from_string("127.0.0.1"), TEST_BASE_PORT + n + i);
      incoming[i] = std::make_shared<CommPartyTCPSynced>(ios[0], in, out);
      outgoing[i] = std::make_shared<CommPartyTCPSynced>(ios[i], out, in);
    }
  }

  // run two methods on two different threads and report the results
  template<typename A, typename B>
  auto launch(A a, B b) -> std::pair<
    decltype(a(std::vector<Channel>())), std::vector<decltype(b(0, nullptr))>
  > {
    auto p0_future = std::async(std::launch::async, [this, a]() {
      for (size_t i = 0; i < n - 1; i++) {
        if (this->incoming[i]->bytesIn + this->incoming[i]->bytesOut > 0) { continue; }
        this->incoming[i]->join(TEST_COMM_SLEEP, TEST_COMM_TIMEOUT);
      }
      return a(this->incoming);
    });

    std::vector<std::future<decltype(b(0, nullptr))>> pi_futures;
    for (size_t i = 1; i < n; i++) {
      pi_futures.push_back(std::async(std::launch::async, [this, i, b]() {
        if (this->outgoing[i - 1]->bytesIn + this->outgoing[i - 1]->bytesOut == 0) {
          this->outgoing[i - 1]->join(TEST_COMM_SLEEP, TEST_COMM_TIMEOUT);
        }
        return b(i, this->outgoing[i - 1]);
      }));
    }

    // collect outputs
    std::vector<decltype(b(0, nullptr))> out;
    for (size_t i = 0; i < n - 1; i++) {
      out.push_back(pi_futures[i].get());
    }
    return {p0_future.get(), out};
  }
};

// text fixture which needs mocked correlations
class MockedCorrelations {
protected:
  std::pair<RandomOTSender, RandomOTReceiver> mockRandomOTs(size_t correlations) {
    std::vector<std::pair<BitString, BitString>> sender;
    std::vector<std::pair<bool, BitString>> receiver;

    BitString b = BitString::sample(correlations);
    for (size_t i = 0; i < correlations; i++) {
      BitString m0 = BitString::sample(RandomOTSender::DEFAULT_ELEMENT_SIZE);
      BitString m1 = BitString::sample(RandomOTSender::DEFAULT_ELEMENT_SIZE);
      BitString mb = b[i] ? m1 : m0;

      sender.push_back(std::make_pair(m0, m1));
      receiver.push_back(std::make_pair(b[i], mb));
    }

    return std::make_pair(RandomOTSender(sender), RandomOTReceiver(receiver));
  }
};
