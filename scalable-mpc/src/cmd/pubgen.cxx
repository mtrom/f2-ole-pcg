#include <iostream>

#include <boost/program_options.hpp>

#include "pkg/lpn.hpp"
#include "util/defines.hpp"
#include "util/timer.hpp"

namespace options = boost::program_options;

void run(const PCGParams& params, const std::string& dir) {

  Timer sample("[pubgen] sample matrices");
  LPN::PrimalMatrix A(params.pkey, params.primal);
  LPN::DualMatrix H(params.dkey, params.dual);
  sample.stop();
  Timer mult("[pubgen] multiply matrices");
  LPN::DenseMatrix B = A * H;
  mult.stop();

  try {
    A.write(dir + "/A.matrix");
    H.write(dir + "/H.matrix");
    B.write(dir + "/B.matrix");

    std::cout << GREEN << "[pubgen] success." << RESET << std::endl;
  } catch (...) {
    std::cout << RED << "[pubgen] failure." << RESET << std::endl;
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
    )
    (
      "dir", options::value<std::string>()->default_value("out/"),
      "writes public matrices to this directory"
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
    std::string dir = vm["dir"].as<std::string>();

    PCGParams params(
      BitString::sample(LAMBDA), 1 << logN, 1 << logk, 1 << logtp, l,
      BitString::sample(LAMBDA), c, td
    );

    run(params, dir);
    return 0;
  } catch (const options::error &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}
