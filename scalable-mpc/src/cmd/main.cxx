#include <iostream>

#include <unordered_map>
#include <boost/program_options.hpp>

#include "pkg/lpn.hpp"
#include "pkg/pprf.hpp"
#include "pkg/pcg.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/random.hpp"
#include "util/timer.hpp"
#include "util/transpose.hpp"

namespace options = boost::program_options;

int main(int argc, char *argv[]) {
  options::variables_map vm;
  options::options_description desc("allowed options");

  // Define the expected options
  desc.add_options()
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
    std::cout << params.toString() << std::endl;

    Timer timer;

    timer.start("sample");
    BitString send_aXeXs(params.dual.N());
    auto send_eXs = PPRF::sample(
      params.dual.t, LAMBDA, params.primal.k, params.dual.blockSize()
    );
    timer.stop();

    timer.start("convert");
    std::vector<unsigned char> raw;
    for (size_t t = 0; t < 8; t++) {
      auto image = send_eXs[t].getImage();
      for (size_t i = 0; i < params.dual.blockSize(); i++) {
        raw.insert(raw.end(), image->operator[](i).begin(), image->operator[](i).end());
      }
    }
    timer.stop();

    timer.start("transpose");
    std::vector<unsigned char> transpose(raw.size());
    sse_trans(raw.data(), transpose.data(), 8 * params.dual.blockSize(), params.primal.k / 8);
    timer.stop();
  } catch (const options::error &ex) {
    std::cerr << "[memcheck] error: " << ex.what() << std::endl;
    return 1;
  }
}
