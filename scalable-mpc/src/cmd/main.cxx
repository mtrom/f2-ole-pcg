#include <iostream>

#include <boost/program_options.hpp>

#include "pkg/lpn.hpp"
#include "pkg/pprf.hpp"
#include "pkg/pcg.hpp"
#include "util/bitstring.hpp"
#include "util/concurrency.hpp"
#include "util/defines.hpp"
#include "util/random.hpp"
#include "util/timer.hpp"

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

    Timer sample("[memcheck] setup");
    PPRF send_eXs = PPRF::sample(params.dual.t, LAMBDA, params.primal.k, params.dual.N());
    sample.stop();
    LPN::PrimalMatrix A = LPN::PrimalMatrix(params.pkey, params.primal);
    LPN::DualMatrix H = LPN::DualMatrix(params.dkey, params.dual);
    LPN::MatrixProduct B = LPN::MatrixProduct(A, H);

    Timer without("[memcheck] without matrix");
    TASK_REDUCE<BitString>(
      [&params, &send_eXs, &A, &B](size_t start, size_t end)
    {
      BitString send_out(end - start);
      for (size_t i = start; i < end; i++) {
        BitString send_aXeXs(params.dual.N());
        for (uint32_t idx : A.getNonZeroElements(i)) {
          // combine columns of the ε ⊗ s matrix
          for (size_t row = 0; row < params.dual.N(); row++) {
            send_aXeXs[row] ^= send_eXs(row)[idx];
          }
        }
        BitString row = B[i];
        send_out[i - start] = row * send_aXeXs;
      }
      return send_out;
    }, [](std::vector<BitString> results) {
      BitString concat;
      for (const BitString& res : results) { concat += res; }
      return concat;
    }, params.size);
    without.stop();

    Timer with("[memcheck] with matrix");
    std::vector<BitString> send_eXs_matrix(params.primal.k, BitString(params.dual.N()));
    MULTI_TASK([&params, &send_eXs, &send_eXs_matrix](size_t start, size_t end) {
      for (size_t c = start; c < end; c++) {
        for (size_t r = 0; r < params.dual.N(); r++) {
          send_eXs_matrix[c][r] ^= send_eXs(r)[c];
        }
      }
    }, params.primal.k);

    TASK_REDUCE<BitString>(
      [&params, &send_eXs_matrix, &A, &B](size_t start, size_t end)
    {
      BitString send_out(end - start);
      for (size_t i = start; i < end; i++) {
        BitString send_aXeXs(params.dual.N());
        for (uint32_t idx : A.getNonZeroElements(i)) {
          send_aXeXs ^= send_eXs_matrix[idx];
        }
        BitString row = B[i];
        send_out[i - start] = row * send_aXeXs;
      }
      return send_out;
    }, [](std::vector<BitString> results) {
      BitString concat;
      for (const BitString& res : results) { concat += res; }
      return concat;
    }, params.size);
    with.stop();

    /*
    std::cout << "[memcheck] starting..." << std::endl;
    Timer a("[memcheck] sample primal");
    LPN::PrimalMatrix A(params.pkey, params.primal);
    a.stop();
    std::cin.get();

    Timer d("[memcheck] sample dual");
    LPN::DualMatrix H(params.dkey, params.dual);
    d.stop();
    std::cin.get();
    */

  } catch (const options::error &ex) {
    std::cerr << "[memcheck] error: " << ex.what() << std::endl;
    return 1;
  }
}
