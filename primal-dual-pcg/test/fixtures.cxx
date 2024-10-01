#include <gtest/gtest.h>

#include <future>
#include <utility>
#include <boost/asio.hpp>

#include "pkg/pcg.hpp"
#include "pkg/rot.hpp"

#define TEST_BASE_PORT 3200

using address = boost::asio::ip::address;

// test fixture to test methods over a channel
class NetworkTest : public testing::Test {
protected:
  // network information for a sender & receiver
  boost::asio::io_service sios, rios;

  // run two methods on two different threads and report the results
  template<typename Sender, typename Receiver>
  auto launch(Sender s, Receiver r) -> std::pair<decltype(s(nullptr)), decltype(r(nullptr))> {
    auto sfuture = std::async(std::launch::async, [this, s]() {
      Channel channel = std::make_shared<TCP>(
        this->sios, address::from_string("127.0.0.1"), TEST_BASE_PORT, TEST_BASE_PORT + 1
      );
      channel->join();
      return s(channel);
    });
    auto rfuture = std::async(std::launch::async, [this, r]() {
      Channel channel = std::make_shared<TCP>(
        this->rios, address::from_string("127.0.0.1"), TEST_BASE_PORT + 1, TEST_BASE_PORT
      );
      channel->join();
      return r(channel);
    });
    return {sfuture.get(), rfuture.get()};
  }
};
