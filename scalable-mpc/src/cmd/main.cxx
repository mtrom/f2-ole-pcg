#include <future>
#include <iostream>
#include <utility>

#include <boost/asio.hpp>

#include "pkg/pcg.hpp"
#include "pkg/rot.hpp"
#include "pkg/svole.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/timer.hpp"


#define BASE_PORT 3200
#define COMM_SLEEP 500
#define COMM_TIMEOUT 5000
const uint32_t ALICE_ID = 0, BOB_ID = 1;

#define ROTS 1024
#define ROT_PORT 3000

using address = boost::asio::ip::address;

void run(const PCGParams& params) {

  SocketPartyData asocket(address::from_string("127.0.0.1"), BASE_PORT);
  SocketPartyData bsocket(address::from_string("127.0.0.1"), BASE_PORT + 1);

  std::cout << "[Run] N = " << params.size << ", k = " << params.primal.k;
  std::cout << ", t = " << params.primal.t << std::endl;

  auto alice = std::async(std::launch::async, [asocket, bsocket, params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, asocket, bsocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    Timer total("Total");

    Timer ots("OT Ext");
    RandomOTSender sender;
    sender.run(ROTS, channel, ROT_PORT);

    RandomOTReceiver receiver;
    receiver.run(ROTS, channel, ROT_PORT);
    ots.stop();

    Beaver::PCG pcg(0, params);

    std::pair<BitString, BitString> output = pcg.run(1, channel, sender, receiver);
    total.stop();

    std::pair<BitString, BitString> inputs = pcg.inputs();
    return std::make_tuple(inputs.first, inputs.second, output.first, output.second);
  });

  auto bob = std::async(std::launch::async, [asocket, bsocket, params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, bsocket, asocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    RandomOTReceiver receiver;
    receiver.run(ROTS, channel, ROT_PORT);

    RandomOTSender sender;
    sender.run(ROTS, channel, ROT_PORT);

    Beaver::PCG pcg(1, params);
    std::pair<BitString, BitString> output = pcg.run(0, channel, sender, receiver);
    std::pair<BitString, BitString> inputs = pcg.inputs();

    return std::make_tuple(inputs.first, inputs.second, output.first, output.second);
  });

  BitString a0, a1, b0, b1, c0, c1, d0, d1;
  std::tie(a0, b0, c0, d0) = alice.get();
  std::tie(a1, b1, c1, d1) = bob.get();

  if ((a0 & b1) == (c0 ^ c1) && (a1 & b0) == (d0 ^ d1)) {
    std::cout << GREEN << "[Run] success." << RESET << std::endl;
  } else {
    std::cout << RED << "[Run] failure." << RESET << std::endl;
  }
}

int main(int argc, char *argv[]) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 10, 1 << 6, 1 << 5, 1 << 3,
    BitString::sample(LAMBDA), 4, 7
  );
  run(params);
}
