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
const uint32_t ALICE_ID = 0, BOB_ID = 1;

using address = boost::asio::ip::address;
namespace options = boost::program_options;

void run(const PCGParams& params) {

  SocketPartyData asocket(address::from_string("127.0.0.1"), BASE_PORT);
  SocketPartyData bsocket(address::from_string("127.0.0.1"), BASE_PORT + 1);

  std::cout << "[Offline] N = " << params.size << ", k = " << params.primal.k;
  std::cout << ", t_p = " << params.primal.t << ", l = " << params.primal.l;
  std::cout << ", c = " << params.dual.c << ", t_d = " << params.dual.t << std::endl;

  auto alice = std::async(std::launch::async, [asocket, bsocket, params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, asocket, bsocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    Timer setup("[Offline] Offline Setup");
    Beaver::PCG pcg(0, params);
    setup.stop();

    Timer ots("[Offline] OT Extension");
    RandomOTSender sender;
    sender.run(pcg.numOTs(), channel, BASE_PORT + 2);

    RandomOTReceiver receiver;
    receiver.run(pcg.numOTs(), channel, BASE_PORT + 2);
    ots.stop();


    Timer run("[Offline] Online Phase");
    std::pair<BitString, BitString> output = pcg.run(1, channel, sender, receiver);
    run.stop();

    float upload = (float) channel->bytesIn / 1000000;
    float download = (float) channel->bytesOut / 1000000;
    std::cout << "[Offline] Upload   = " << upload << "MB" << std::endl;
    std::cout << "          Download = " << download << "MB" << std::endl;
    std::cout << "          Total    = " << (upload + download) << "MB" << std::endl;

    std::pair<BitString, BitString> inputs = pcg.inputs();
    return std::make_tuple(inputs.first, inputs.second, output.first, output.second);
  });

  auto bob = std::async(std::launch::async, [asocket, bsocket, params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<CommPartyTCPSynced>(ios, bsocket, asocket);
    channel->join(COMM_SLEEP, COMM_TIMEOUT);

    Beaver::PCG pcg(1, params);

    RandomOTReceiver receiver;
    receiver.run(pcg.numOTs(), channel, BASE_PORT + 2);

    RandomOTSender sender;
    sender.run(pcg.numOTs(), channel, BASE_PORT + 2);

    std::pair<BitString, BitString> output = pcg.run(0, channel, sender, receiver);
    std::pair<BitString, BitString> inputs = pcg.inputs();

    return std::make_tuple(inputs.first, inputs.second, output.first, output.second);
  });

  BitString a0, a1, b0, b1, c0, c1, d0, d1;
  std::tie(a0, b0, c0, d0) = alice.get();
  std::tie(a1, b1, c1, d1) = bob.get();

  if ((a0 & b1) == (c0 ^ c1) && (a1 & b0) == (d0 ^ d1)) {
    std::cout << GREEN << "[Offline] success." << RESET << std::endl;
  } else {
    std::cout << RED << "[Offline] failure." << RESET << std::endl;
  }
}

int main(int argc, char *argv[]) {

  options::variables_map vm;
  options::options_description desc("allowed options");

  // Define the expected options
  desc.add_options()
    ("help,h", "Display help message")
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

    run(params);
    return 0;
  } catch (const options::error &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}
