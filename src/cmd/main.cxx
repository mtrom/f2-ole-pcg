#include <future>
#include <iostream>
#include <utility>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "pkg/pcg.hpp"
#include "pkg/rot.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/timer.hpp"

#define BASE_PORT 3200
#define OT_EXT_PORT 3300

using address = boost::asio::ip::address;
namespace options = boost::program_options;

void run(const PCGParams& params, const std::string& host, bool send) {
  Timer timer;

  boost::asio::io_service ios;
  Channel channel = std::make_shared<TCP>(ios, address::from_string(host), BASE_PORT);

  std::cout << params.toString() << std::endl << std::endl;

  std::unique_ptr<PCG::Base> pcg;
  if (send) { pcg = std::make_unique<PCG::Sender>(params); }
  else      { pcg = std::make_unique<PCG::Receiver>(params); }

  pcg->init();

  timer.start("[protocol] prepare");
  pcg->prepare();
  timer.stop();

  channel->join();

  timer.start("[protocol] online");
  size_t upload, download;

  // create the mocked random ots for use in the protocol
  auto [srots, rrots] = pcg->numOTs();
  ROT::Sender sender;
  ROT::Receiver receiver;
  if (send) {
    auto comms = sender.run(srots, "0.0.0.0", OT_EXT_PORT);
    upload += comms.first;
    download += comms.second;
    comms = receiver.run(rrots, host, OT_EXT_PORT);
    upload += comms.first;
    download += comms.second;
  } else {
    auto comms = receiver.run(rrots, host, OT_EXT_PORT);
    upload += comms.first;
    download += comms.second;
    comms = sender.run(srots, "0.0.0.0", OT_EXT_PORT);
    upload += comms.first;
    download += comms.second;
  }

  pcg->online(channel, sender, receiver);
  timer.stop();

  upload += channel->upload();
  download += channel->download();
  float uploadMB = (float) upload / (size_t) (1 << 20);
  float downloadMB = (float) download / (size_t) (1 << 20);
    std::cout << "           upload       : " << uploadMB << " MB" << std::endl;
    std::cout << "           download     : " << downloadMB << " MB" << std::endl;
    std::cout << "           total        : " << (uploadMB + downloadMB) << " MB" << std::endl;

  channel.reset();

  // free public matrices for memory purposes
  // (allows for larger parameters to be run)
  pcg->clear();

  timer.start("[protocol] finalize");
  pcg->finalize();
  timer.stop();

  // resample public matrices
  pcg->init();

  timer.start("[ expand ] expand");
  pcg->expand();
  timer.stop();

  std::cout << GREEN << "[  done  ] success." << RESET << std::endl;
}

void runBoth(const PCGParams& params) {
  std::cout << params.toString() << std::endl << std::endl;

  auto alice = std::async(std::launch::async, [params]() {
    Timer timer;
    boost::asio::io_service ios;
    Channel channel = std::make_shared<TCP>(
      ios, address::from_string("127.0.0.1"), BASE_PORT, BASE_PORT + 1
    );
    channel->join();

    PCG::Sender pcg(params);
    pcg.init();

    timer.start("[protocol] prepare");
    pcg.prepare();
    timer.stop();

    timer.start("[protocol] online");
    size_t upload, download;

    // create the mocked random ots for use in the protocol
    auto [srots, rrots] = pcg.numOTs();
    ROT::Sender sender;
    ROT::Receiver receiver;
    auto comms = sender.run(srots, "127.0.0.1", OT_EXT_PORT);
    upload += comms.first;
    download += comms.second;
    comms = receiver.run(rrots, "127.0.0.1", OT_EXT_PORT);
    upload += comms.first;
    download += comms.second;
    pcg.online(channel, sender, receiver);
    timer.stop();

    upload += channel->upload();
    download += channel->download();
    float uploadMB = (float) upload / (size_t) (1 << 20);
    float downloadMB = (float) download / (size_t) (1 << 20);
    std::cout << "           upload       : " << uploadMB << " MB" << std::endl;
    std::cout << "           download     : " << downloadMB << " MB" << std::endl;
    std::cout << "           total        : " << (uploadMB + downloadMB) << " MB" << std::endl;

    timer.start("[protocol] online");
    pcg.finalize();
    timer.stop();

    timer.start("[ expand ] expand");
    pcg.expand();
    timer.stop();

    BitString inputs = pcg.inputs();
    return std::make_tuple(inputs, pcg.output);
  });

  auto bob = std::async(std::launch::async, [params]() {
    boost::asio::io_service ios;
    Channel channel = std::make_shared<TCP>(
      ios, address::from_string("127.0.0.1"), BASE_PORT + 1, BASE_PORT
    );
    channel->join();

    PCG::Receiver pcg(params);
    pcg.init();

    auto [srots, rrots] = pcg.numOTs();
    ROT::Sender sender;
    ROT::Receiver receiver;
    receiver.run(rrots, "127.0.0.1", OT_EXT_PORT);
    sender.run(srots, "127.0.0.1", OT_EXT_PORT);

    BitString output = pcg.run(channel, sender, receiver);
    BitString inputs = pcg.inputs();

    return std::make_tuple(inputs, output);
  });

  BitString a, b, c0, c1;
  std::tie(a, c0) = alice.get();
  std::tie(b, c1) = bob.get();

  if ((a & b) == (c0 ^ c1)) {
    std::cout << GREEN << "[  done  ] success." << RESET << std::endl;
  } else {
    std::cout << RED << "[  done  ] failure." << RESET << std::endl;
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
      std::cerr << "[protocol] to run protocol with both parties use --both flag" << std::endl;
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
      run(params, host, true);
    } else if (recv) {
      run(params, host, false);
    } else {
      std::cerr << "[protocol] need one of --send, --recv, or --both to be true" << std::endl;
    }
    return 0;
  } catch (const options::error &ex) {
    std::cerr << "[protocol] error: " << ex.what() << std::endl;
    return 1;
  }
}
