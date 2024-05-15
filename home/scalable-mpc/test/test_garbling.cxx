#include <gtest/gtest.h>

#include "pkg/garbling.hpp"
#include "util/defines.hpp"
#include "./fixtures.cxx"

// number of parties in these tests
const int N = 3;

class GarblingTests : public MultiNetworkTest, public MockedCorrelations {
public:
  GarblingTests() : MultiNetworkTest(N) { }
};

TEST_F(GarblingTests, RunSimpleCircuit) {
  Gate g0(0, { 0, 1, 1, 0 }, { 0, 1 }, { 4 }); // w0 ^ w1 = w4
  Gate g1(1, { 0, 0, 0, 1 }, { 2, 3 }, { 5 }); // w2 & w3 = w5
  Gate g2(2, { 0, 0, 0, 1 }, { 4, 5 }, { 6 }); // w4 & w5 = w6
  BooleanCircuit bc({ g0, g1, g2 }, std::vector<int>{ 6 }, { {0, 1, 2, 3} });
  auto cost = Circuit(bc).correlationsCost();

  std::vector<BitString> inputs({
    BitString("0000"), BitString("1000"), BitString("0100"), BitString("1100"),
    BitString("0010"), BitString("1010"), BitString("0110"), BitString("1110"),
    BitString("0001"), BitString("1001"), BitString("0101"), BitString("1101"),
    BitString("0011"), BitString("1011"), BitString("0111"), BitString("1111")
  });

  for (const BitString& input : inputs) {
    std::vector<Beaver::Triples> triples = this->mockTriples(N, cost.first);
    std::vector<sVOLE> svoles = this->mocksVOLE(N, LABEL_SIZE, cost.second);

    auto results = this->launch(
      [&](std::vector<Channel> channels) -> bool {
        Evaluator evaluator(channels, triples[0], svoles[0]);
        Circuit circuit(bc, true);
        BitString values(2);
        values[0] = input[0];
        values[1] = input[3];
        return evaluator.run(std::move(circuit), values)[0];
      },
      [&](int id, Channel channel) -> bool {
        Garbler garbler(id, N, channel, triples[id], svoles[id]);
        Circuit circuit(bc);
        BitString values(1);
        values[0] = input[id];
        garbler.run(std::move(circuit), values);
        return true;
      }
    );

    ASSERT_EQ(results.first, (input[0] ^ input[1]) && (input[2] && input[3]));
  }
}

TEST_F(GarblingTests, RunSimpleCircuitWithNot) {
  Gate g0(0, { 0, 1, 1, 0 }, { 0, 1 }, { 3 }); // w0 ^ w1 = w2
  Gate g1(1, { 1, 0 }, { 2 }, { 4 });          // !w2 = w4
  Gate g2(2, { 0, 0, 0, 1 }, { 3, 4 }, { 5 }); // w4 & w5 = w6
  BooleanCircuit bc({ g0, g1, g2 }, std::vector<int>{ 5 }, { {0, 1, 2 } });
  auto cost = Circuit(bc).correlationsCost();

  std::vector<BitString> inputs({
    BitString("000"), BitString("100"), BitString("010"), BitString("110"),
    BitString("001"), BitString("101"), BitString("011"), BitString("111"),
    BitString("000"), BitString("100"), BitString("010"), BitString("110"),
    BitString("001"), BitString("101"), BitString("011"), BitString("111")
  });

  for (const BitString& input : inputs) {
    std::vector<Beaver::Triples> triples = this->mockTriples(N, cost.first);
    std::vector<sVOLE> svoles = this->mocksVOLE(N, LABEL_SIZE, cost.second);

    auto results = this->launch(
      [&](std::vector<Channel> channels) -> bool {
        Evaluator evaluator(channels, triples[0], svoles[0]);
        Circuit circuit(bc, true);
        BitString values(1);
        values[0] = input[0];
        return evaluator.run(std::move(circuit), values)[0];
      },
      [&](int id, Channel channel) -> bool {
        Garbler garbler(id, N, channel, triples[id], svoles[id]);
        Circuit circuit(bc);
        BitString values(1);
        values[0] = input[id];
        garbler.run(std::move(circuit), values);
        return true;
      }
    );

    ASSERT_EQ(results.first, (input[1] ^ input[2]) && !input[0]);
  }
}
