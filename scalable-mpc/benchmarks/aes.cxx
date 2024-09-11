#include <gtest/gtest.h>

#include "pkg/garbling.hpp"
#include "util/defines.hpp"
#include "../test/fixtures.cxx"

// number of parties in the benchmark
const int N = 3;

class Benchmarks : public MultiNetworkTest, public MockedCorrelations {
public:
  Benchmarks() : MultiNetworkTest(N) { }
};

TEST_F(Benchmarks, AES) {
  std::string fn = "../test/AES.txt";
  auto cost = Circuit(fn).correlationsCost();

  std::vector<Beaver::Triples> triples = this->mockTriples(N, cost.first);
  std::vector<sVOLE> svoles = this->mocksVOLE(N, LABEL_SIZE, cost.second);

  auto results = this->launch(
    [&](std::vector<Channel> channels) -> bool {
      Evaluator evaluator(channels, triples[0], svoles[0]);
      Circuit circuit(fn, true);
      // just sample random inputs
      BitString values = BitString::sample(circuit.inputs().size());
      return evaluator.run(std::move(circuit), values)[0];
    },
    [&](int id, Channel channel) -> bool {
      Garbler garbler(id, N, channel, triples[id], svoles[id]);
      Circuit circuit(fn);
      // just sample random inputs
      BitString values = BitString::sample(circuit.inputs().size());
      garbler.run(std::move(circuit), values);
      return true;
    }
  );
  for (Channel channel : incoming) {
    float upload = (float) channel->bytesIn / 1000000;
    float download = (float) channel->bytesOut / 1000000;
    std::cout << ">>>>>>>>> PARTY 0 <<<<<<<<<" << std::endl;
    std::cout << "[Benchmarks.AES] Upload   = " << upload << "MB" << std::endl;
    std::cout << "                 Download = " << download << "MB" << std::endl;
    std::cout << "                 Total    = " << (upload + download) << "MB" << std::endl;
  }

  uint32_t i = 1;
  for (Channel channel : outgoing) {
    float upload = (float) channel->bytesIn / 1000000;
    float download = (float) channel->bytesOut / 1000000;
    std::cout << ">>>>>>>>> PARTY " << i << " <<<<<<<<<" << std::endl;
    std::cout << "[Benchmarks.AES] Upload   = " << upload << "MB";
    std::cout << " (" << channel->bytesIn << "B)" << std::endl;
    std::cout << "                 Download = " << download << "MB";
    std::cout << " (" << channel->bytesOut << "B)" << std::endl;
    std::cout << "                 Total    = " << (upload + download) << "MB";
    std::cout << " (" << channel->bytesIn + channel->bytesOut << "B)" << std::endl;
    i++;
  }

}
