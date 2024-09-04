#include <gtest/gtest.h>

#include <future>
#include <utility>
#include <boost/asio.hpp>
#include <libscapi/include/comm/Comm.hpp>

#include "pkg/pcg.hpp"
#include "pkg/rot.hpp"
#include "pkg/svole.hpp"

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
  const size_t ROTS = 1 << 20;

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
  std::vector<Beaver::Triples> mockTriples(int parties, size_t triples) {
    std::vector<Beaver::MockPCG> pcgs;
    for (size_t id = 0; id < parties; id++) {
      pcgs.push_back(Beaver::MockPCG(id, triples));
    }

    // "run" the 2-party protocol between all parties
    std::vector<Beaver::Triples> output;
    for (size_t i = 0; i < parties; i++) {
      std::vector<BitString> c, d, seeds;
      for (size_t j = 0; j < parties; j++) {
        if (i == j) { continue; }
        BitString cij, dji, seed;
        std::tie(seed, cij, dji) = pcgs[i].run(j);
        c.push_back(cij);
        d.push_back(dji);
        seeds.push_back(seed);
      }
      // convert the 2-party correlations to n-party
      BitString a, b;
      std::tie(a, b) = pcgs[i].inputs();
      output.push_back(Beaver::transform(a, b, c, d, seeds));
    }

    return output;
  }

  std::vector<sVOLE> mocksVOLE(int parties, size_t delta_size, size_t correlations) {
    std::vector<sVOLE> out;

    // values being shared
    BitString s(correlations);
    BitString delta(delta_size);
    std::vector<BitString> u;
    for (size_t j = 0; j < correlations; j++) {
      u.push_back(BitString(delta_size));
    }

    // all but the last party randomly samples shares
    for (size_t i = 0; i < parties - 1; i++) {
      BitString s_i = BitString::sample(correlations);
      s ^= s_i;

      BitString delta_i = BitString::sample(delta_size);
      delta ^= delta_i;

      std::vector<BitString> u_i;
      for (size_t j = 0; j < correlations; j++) {
        u_i.push_back(BitString::sample(delta_size));
        u[j] ^= u_i[j];
      }
      out.push_back(sVOLE(delta_i, s_i, u_i));
    }

    // last party samples their share of s & delta...
    BitString s_n = BitString::sample(correlations);
    s ^= s_n;
    BitString delta_n = BitString::sample(delta_size);
    delta ^= delta_n;

    // ... but computes their share of u to make the correlation work
    std::vector<BitString> u_n(correlations);
    for (size_t i = 0; i < correlations; i++) {
      if (s[i]) {
        u_n[i] = u[i] ^ delta;
      } else {
        u_n[i] = u[i];
      }
    }

    out.push_back(sVOLE(delta_n, s_n, u_n));
    return out;
  }

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
