#include <future>
#include <iostream>
#include <utility>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "pkg/pcg.hpp"
#include "pkg/rot.hpp"
#include "pkg/svole.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/timer.hpp"

#define BASE_PORT 3200
#define COMM_SLEEP 500
#define COMM_TIMEOUT 5000
#define SEND_ID 0
#define RECV_ID 1


using address = boost::asio::ip::address;
namespace options = boost::program_options;

void runSender(const PCGParams& params, const std::string& host) {
  std::cout << params.toString() << std::endl;

  SocketPartyData my_socket(address::from_string("0.0.0.0"), BASE_PORT);
  SocketPartyData their_socket(address::from_string(host), BASE_PORT);

  Timer setup("[offline] setup");
  Beaver::PCG pcg(SEND_ID, params);
  setup.stop();

  Timer prepare("[offline] prepare");
  pcg.prepare();
  prepare.stop();

  boost::asio::io_service ios;
  Channel channel = std::make_shared<CommPartyTCPSynced>(ios, my_socket, their_socket);
  channel->join(COMM_SLEEP, COMM_TIMEOUT);

  Timer online("[offline] online");
  Timer ots("[online] ot ext");
  size_t srots, rrots;
  std::tie(srots, rrots) = pcg.numOTs(RECV_ID);

  RandomOTSender sender;
  sender.run(srots, channel, BASE_PORT + 2);

  RandomOTReceiver receiver;
  receiver.run(rrots, channel, host, BASE_PORT + 2);
  ots.stop();

  pcg.online(RECV_ID, channel, sender, receiver);
  online.stop();

  float upload = (float) channel->bytesIn / 1000000;
  float download = (float) channel->bytesOut / 1000000;
  std::cout << "[offline] upload   = " << upload << "MB" << std::endl;
  std::cout << "          download = " << download << "MB" << std::endl;
  std::cout << "          total    = " << (upload + download) << "MB" << std::endl;

  channel.reset();

  Timer finalize("[offline] finalize");
  std::pair<BitString, BitString> output = pcg.finalize(RECV_ID);
  finalize.stop();

  std::cout << GREEN << "[offline] done." << RESET << std::endl;
}

void runReceiver(const PCGParams& params, const std::string& host) {
  std::cout << params.toString();

  SocketPartyData my_socket(address::from_string("0.0.0.0"), BASE_PORT);
  SocketPartyData their_socket(address::from_string(host), BASE_PORT);

  Timer setup("[offline] setup");
  Beaver::PCG pcg(RECV_ID, params);
  setup.stop();

  Timer prepare("[offline] prepare");
  pcg.prepare();
  prepare.stop();

  boost::asio::io_service ios;
  Channel channel = std::make_shared<CommPartyTCPSynced>(ios, my_socket, their_socket);
  channel->join(COMM_SLEEP, COMM_TIMEOUT);

  Timer online("[offline] online");
  Timer ots("[online] ot ext");
  size_t srots, rrots;
  std::tie(srots, rrots) = pcg.numOTs(SEND_ID);

  RandomOTReceiver receiver;
  receiver.run(rrots, channel, host, BASE_PORT + 2);

  RandomOTSender sender;
  sender.run(srots, channel, BASE_PORT + 2);
  ots.stop();

  pcg.online(SEND_ID, channel, sender, receiver);
  online.stop();

  float upload = (float) channel->bytesIn / 1000000;
  float download = (float) channel->bytesOut / 1000000;
  std::cout << "[offline] upload   = " << upload << "MB" << std::endl;
  std::cout << "          download = " << download << "MB" << std::endl;
  std::cout << "          total    = " << (upload + download) << "MB" << std::endl;

  channel.reset();

  Timer finalize("[offline] finalize");
  std::pair<BitString, BitString> output = pcg.finalize(SEND_ID);
  finalize.stop();

  std::cout << GREEN << "[offline] done." << RESET << std::endl;
}

void runBoth(const PCGParams& params) {

  SocketPartyData asocket(address::from_string("127.0.0.1"), BASE_PORT);
  SocketPartyData bsocket(address::from_string("127.0.0.1"), BASE_PORT + 1);

  std::cout << "[offline] N = " << params.size << ", k = " << params.primal.k;
  std::cout << ", t_p = " << params.primal.t << ", l = " << params.primal.l;
  std::cout << ", c = " << params.dual.c << ", t_d = " << params.dual.t << std::endl;

  auto alice = std::async(std::launch::async, [asocket, bsocket, params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, asocket, bsocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    Beaver::PCG pcg(SEND_ID, params);

    Timer ots("[offline] ot ext");
    size_t srots, rrots;
    std::tie(srots, rrots) = pcg.numOTs(RECV_ID);

    RandomOTSender sender;
    sender.run(srots, channel, BASE_PORT + 2);

    RandomOTReceiver receiver;
    receiver.run(rrots, channel, "localhost", BASE_PORT + 2);
    ots.stop();

    Timer prepare("[offline] prepare");
    pcg.prepare();
    prepare.stop();

    Timer online("[offline] online");
    pcg.online(RECV_ID, channel, sender, receiver);
    online.stop();

    Timer finalize("[offline] finalize");
    std::pair<BitString, BitString> output = pcg.finalize(RECV_ID);
    finalize.stop();

    float upload = (float) channel->bytesIn / 1000000;
    float download = (float) channel->bytesOut / 1000000;
    std::cout << "[offline] Upload   = " << upload << "MB" << std::endl;
    std::cout << "          Download = " << download << "MB" << std::endl;
    std::cout << "          Total    = " << (upload + download) << "MB" << std::endl;

    std::pair<BitString, BitString> inputs = pcg.inputs();
    return std::make_tuple(inputs.first, inputs.second, output.first, output.second);
  });

  auto bob = std::async(std::launch::async, [asocket, bsocket, params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, bsocket, asocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    Beaver::PCG pcg(RECV_ID, params);

    size_t srots, rrots;
    std::tie(srots, rrots) = pcg.numOTs(SEND_ID);

    RandomOTReceiver receiver;
    receiver.run(rrots, channel, "localhost", BASE_PORT + 2);

    RandomOTSender sender;
    sender.run(srots, channel, BASE_PORT + 2);

    std::pair<BitString, BitString> output = pcg.run(SEND_ID, channel, sender, receiver);
    std::pair<BitString, BitString> inputs = pcg.inputs();

    return std::make_tuple(inputs.first, inputs.second, output.first, output.second);
  });

  BitString a0, a1, b0, b1, c0, c1, d0, d1;
  std::tie(a0, b0, c0, d0) = alice.get();
  std::tie(a1, b1, c1, d1) = bob.get();

  if ((a0 & b1) == (c0 ^ c1) && (a1 & b0) == (d0 ^ d1)) {
    std::cout << GREEN << "[offline] success." << RESET << std::endl;
  } else {
    std::cout << RED << "[offline] failure." << RESET << std::endl;
  }
}

int main(int argc, char *argv[]) {

  options::variables_map vm;
  options::options_description desc("allowed options");

  // Define the expected options
  desc.add_options()
    ("help,h", "Display help message")
    ("send", options::bool_switch(), "run protocol as the sender")
    ("recv", options::bool_switch(), "run protocol as the receiver")
    ("both", options::bool_switch(), "run protocol as both parties")
    (
      "host", options::value<std::string>()->default_value("127.0.0.1"),
      "the other party's public ip address"
    )
    ("logN", options::value<unsigned>()->required(), "log of the number of triples to generate")
    ("logk", options::value<unsigned>()->required(), "log of the size of primal LPN secret vector")
    ("logtp", options::value<unsigned>()->required(), "log of the primal LPN error vector weight")
    ("l", options::value<unsigned>()->required(), "row weight for primal LPN matrix")
    ("c", options::value<unsigned>()->default_value(4), "compression rate of dual LPN")
    (
      "td", options::value<unsigned>()->default_value(32),
      "dual LPN error vector weight"
    );

  try {
    options::store(options::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
    }

    options::notify(vm);

    bool send = vm["send"].as<bool>();
    bool recv = vm["recv"].as<bool>();
    bool both = vm["both"].as<bool>();

    if (send && recv) {
      std::cerr << "[offline] to run protocol with both parties use --both flag" << std::endl;
    }

    std::string host = vm["host"].as<std::string>();

    unsigned logN = vm["logN"].as<unsigned>();
    unsigned logk = vm["logk"].as<unsigned>();
    unsigned logtp = vm["logtp"].as<unsigned>();
    unsigned l = vm["l"].as<unsigned>();
    unsigned c = vm["c"].as<unsigned>();
    unsigned td = vm["td"].as<unsigned>();

    PCGParams params(
      BitString::sample(LAMBDA), 1 << logN, 1 << logk, 1 << logtp, l,
      BitString::sample(LAMBDA), c, td
    );

    if (both) {
      runBoth(params);
    } else if (send) {
      runSender(params, host);
    } else if (recv) {
      runReceiver(params, host);
    } else {
      std::cerr << "[offline] need one of --send, --recv, or --both to be true" << std::endl;
    }
    return 0;
  } catch (const options::error &ex) {
    std::cerr << "[offline] error: " << ex.what() << std::endl;
    return 1;
  }
}
