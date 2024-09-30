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


using address = boost::asio::ip::address;
namespace options = boost::program_options;

void runSender(const PCGParams& params, const std::string& host) {
  std::cout << params.toString() << std::endl;

  Timer timer;
  SocketPartyData my_socket(address::from_string("0.0.0.0"), BASE_PORT);
  SocketPartyData their_socket(address::from_string(host), BASE_PORT);

  Beaver::Sender pcg(params);

  timer.start("[offline] init");
  pcg.init();
  timer.stop();

  timer.start("[offline] prepare");
  pcg.prepare();
  timer.stop();

  boost::asio::io_service ios;
  Channel channel = std::make_shared<CommPartyTCPSynced>(ios, my_socket, their_socket);
  channel->join(COMM_SLEEP, COMM_TIMEOUT);

  timer.start("[offline] online");
  Timer subtimer("[online] ot ext");
  size_t srots, rrots;
  std::tie(srots, rrots) = pcg.numOTs();

  RandomOTSender sender;
  sender.run(srots, channel, BASE_PORT + 2);

  RandomOTReceiver receiver;
  receiver.run(rrots, channel, host, BASE_PORT + 2);
  subtimer.stop();

  pcg.online(channel, sender, receiver);
  timer.stop();

  float upload = (float) channel->bytesIn / (size_t) (1 << 20);
  float download = (float) channel->bytesOut / (size_t) (1 << 20);
  std::cout << "          upload   = " << upload << "MB" << std::endl;
  std::cout << "          download = " << download << "MB" << std::endl;
  std::cout << "          total    = " << (upload + download) << "MB" << std::endl;

  channel.reset();

  // free public matrices for memory purposes
  pcg.clear();

  timer.start("[offline] finalize");
  pcg.finalize();
  timer.stop();

  timer.start("[offline] reinit");
  pcg.init();
  timer.stop();

  timer.start("[offline] expand");
  pcg.expand();
  timer.stop();

  std::cout << GREEN << "[offline] done." << RESET << std::endl;
}

void runReceiver(const PCGParams& params, const std::string& host) {
  std::cout << params.toString() << std::endl;
  Timer timer;

  SocketPartyData my_socket(address::from_string("0.0.0.0"), BASE_PORT);
  SocketPartyData their_socket(address::from_string(host), BASE_PORT);

  timer.start("[offline] setup");
  Beaver::Receiver pcg(params);
  timer.stop();

  timer.start("[offline] init");
  pcg.init();
  timer.stop();

  timer.start("[offline] prepare");
  pcg.prepare();
  timer.stop();

  boost::asio::io_service ios;
  Channel channel = std::make_shared<CommPartyTCPSynced>(ios, my_socket, their_socket);
  channel->join(COMM_SLEEP, COMM_TIMEOUT);

  timer.start("[offline] online");
  Timer subtimer("[online] ot ext");
  size_t srots, rrots;
  std::tie(srots, rrots) = pcg.numOTs();

  RandomOTReceiver receiver;
  receiver.run(rrots, channel, host, BASE_PORT + 2);

  RandomOTSender sender;
  sender.run(srots, channel, BASE_PORT + 2);
  subtimer.stop();

  pcg.online(channel, sender, receiver);
  timer.stop();

  float upload = (float) channel->bytesIn / (size_t) (1 << 20);
  float download = (float) channel->bytesOut / (size_t) (1 << 20);
  std::cout << "          upload   = " << upload << "MB" << std::endl;
  std::cout << "          download = " << download << "MB" << std::endl;
  std::cout << "          total    = " << (upload + download) << "MB" << std::endl;

  channel.reset();

  // free public matrices for memory purposes
  pcg.clear();

  timer.start("[offline] finalize");
  pcg.finalize();
  timer.stop();

  timer.start("[offline] reinit");
  pcg.init();
  timer.stop();

  timer.start("[offline] expand");
  pcg.expand();
  timer.stop();

  std::cout << GREEN << "[offline] done." << RESET << std::endl;
}

void runBoth(const PCGParams& params) {
  std::cout << params.toString();

  SocketPartyData asocket(address::from_string("127.0.0.1"), BASE_PORT);
  SocketPartyData bsocket(address::from_string("127.0.0.1"), BASE_PORT + 1);

  auto alice = std::async(std::launch::async, [asocket, bsocket, params]() {
    Timer timer;
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, asocket, bsocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    Beaver::Sender pcg(params);
    pcg.init();

    timer.start("[offline] ot ext");
    size_t srots, rrots;
    std::tie(srots, rrots) = pcg.numOTs();

    RandomOTSender sender;
    sender.run(srots, channel, BASE_PORT + 2);

    RandomOTReceiver receiver;
    receiver.run(rrots, channel, "localhost", BASE_PORT + 2);
    timer.stop();

    timer.start("[offline] prepare");
    pcg.prepare();
    timer.stop();

    timer.start("[offline] online");
    pcg.online(channel, sender, receiver);
    timer.stop();

    float upload = (float) channel->bytesIn / 1000000;
    float download = (float) channel->bytesOut / 1000000;
    std::cout << "          upload   = " << upload << "MB" << std::endl;
    std::cout << "          download = " << download << "MB" << std::endl;
    std::cout << "          total    = " << (upload + download) << "MB" << std::endl;

    timer.start("[offline] finalize");
    pcg.finalize();
    timer.stop();

    timer.start("[offline] expand");
    pcg.expand();
    timer.stop();

    BitString inputs = pcg.inputs();
    return std::make_tuple(inputs, pcg.output);
  });

  auto bob = std::async(std::launch::async, [asocket, bsocket, params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, bsocket, asocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    Beaver::Receiver pcg(params);
    pcg.init();

    size_t srots, rrots;
    std::tie(srots, rrots) = pcg.numOTs();

    RandomOTReceiver receiver;
    receiver.run(rrots, channel, "localhost", BASE_PORT + 2);

    RandomOTSender sender;
    sender.run(srots, channel, BASE_PORT + 2);

    BitString output = pcg.run(channel, sender, receiver);
    BitString inputs = pcg.inputs();

    return std::make_tuple(inputs, output);
  });

  BitString a, b, c0, c1;
  std::tie(a, c0) = alice.get();
  std::tie(b, c1) = bob.get();

  if ((a & b) == (c0 ^ c1)) {
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
    ("logCorrelations", options::value<unsigned>()->default_value(0), "log the number of correlations to compute")
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

    unsigned logC = vm["logCorrelations"].as<unsigned>();
    unsigned logN = vm["logN"].as<unsigned>();
    unsigned logk = vm["logk"].as<unsigned>();
    unsigned logtp = vm["logtp"].as<unsigned>();
    unsigned l = vm["l"].as<unsigned>();
    unsigned c = vm["c"].as<unsigned>();
    unsigned td = vm["td"].as<unsigned>();

    if (logC == 0) { logC = logN; }

    PCGParams params(
      1 << logC,
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
