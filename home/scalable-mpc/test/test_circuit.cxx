#include <gtest/gtest.h>

// lets us test private / protected methods and fields
#define private public
#define protected public

#include "util/circuit.hpp"
#include "./fixtures.cxx"

class CircuitTests : public MockedCorrelations, public testing::Test { };

TEST_F(CircuitTests, SimpleBooleanCircuit) {
  Gate g0(0, { 0, 1, 1, 0 }, { 0, 1 }, { 4 }); // w0 ^ w1 = w4
  Gate g1(1, { 0, 0, 0, 1 }, { 2, 3 }, { 5 }); // w2 & w3 = w5
  Gate g2(2, { 0, 0, 0, 1 }, { 4, 5 }, { 6 }); // w4 & w5 = w6
  BooleanCircuit bc({ g0, g1, g2 }, std::vector<int>{ 6 }, { {0, 1, 2, 3} });
  Circuit circuit(bc);

  ASSERT_EQ(circuit.outputs(), std::vector<int>({ 6 }));
  ASSERT_EQ(circuit.inputs(), std::vector<int>({0, 1, 2, 3}));

  int expected = 0;
  for (LogicGate gate : circuit.getGates()) {
    ASSERT_EQ(gate.id, expected);
    expected++;
  }

  expected = 2;
  for (LogicGate gate : circuit.getGatesReversed()) {
    ASSERT_EQ(gate.id, expected);
    expected--;
  }
}

TEST_F(CircuitTests, GetGates) {
  Circuit circuit("../test/AES.txt");

  std::set<int> wires;
  for (int input : circuit.inputs()) { wires.insert(input); }

  // confirm that getGates is indeed topological
  for (LogicGate gate : circuit.getGates()) {
    // input wires have been seen already
    ASSERT_FALSE(wires.find(gate.inputs.first) == wires.end());
    ASSERT_FALSE(wires.find(gate.inputs.second) == wires.end());

    // output wire has not been seen
    ASSERT_TRUE(wires.find(gate.output) == wires.end());

    wires.insert(gate.output);
  }
}

TEST_F(CircuitTests, GetGatesReversed) {
  Circuit circuit("../test/AES.txt");

  std::set<int> wires;
  for (int output : circuit.outputs()) { wires.insert(output); }

  // confirm that getGatesReversed is indeed reverse topological
  for (LogicGate gate : circuit.getGatesReversed()) {
    // output wire has been seen already
    ASSERT_FALSE(wires.find(gate.output) == wires.end());

    wires.insert(gate.inputs.first);
    wires.insert(gate.inputs.second);
  }
}

TEST_F(CircuitTests, AllIdsUsed) {
  Circuit circuit("../test/AES.txt");

  std::set<int> wires;
  std::set<int> gates;

  // confirm that getGates is indeed topological
  for (LogicGate gate : circuit.getGates()) {
    wires.insert(gate.output);
    wires.insert(gate.inputs.first);
    wires.insert(gate.inputs.second);
    gates.insert(gate.id);
  }

  // all numbers in [-1, wires.size() - 1) should be used as an id somewhere
  for (int i = -1; i < wires.size() - 1; i++) {
    ASSERT_FALSE(wires.find(i) == wires.end());
  }

  // similarly for gates
  for (int i = 0; i < gates.size(); i++) {
    ASSERT_FALSE(gates.find(i) == gates.end());
  }
}

TEST_F(CircuitTests, Prepare) {
  Circuit circuit = Circuit("../test/AES.txt", true);
  std::pair<size_t, size_t> cost = circuit.correlationsCost();
  std::vector<Beaver::Triples> triples = this->mockTriples(3, cost.first);

  BitString p0 = circuit.prepare(&triples[0], BitString::sample(LABEL_SIZE));
  BitString p1 = Circuit("../test/AES.txt").prepare(&triples[1], BitString::sample(LABEL_SIZE));
  BitString p2 = Circuit("../test/AES.txt").prepare(&triples[2], BitString::sample(LABEL_SIZE));

  // this should be 𝛾 = g(α ^ rᵤ, β ^ rᵥ) ^ rₒ ∀ α,β for all AND gates
  BitString reconstructed = p0 ^ p1 ^ p2;

  ASSERT_EQ(reconstructed.size() % 4, 0);
  for (size_t i = 0; i < 100; i += 4) {
    // each shuffled AND truth table only has a single 1 before masking
    //  and then either 1 or 3 after masking
    BitString tt = reconstructed[{i, i + 4}];
    ASSERT_EQ(tt.weight() % 2, 1);
  }
}

TEST_F(CircuitTests, GarbleSmallCircuit) {
  Gate g0(0, { 0, 0, 0, 1 }, { 0, 1 }, { 2 }); // w0 ^ w1 = w4
  BooleanCircuit bc(std::vector<Gate>{ g0 }, std::vector<int>{ 2 }, { {0, 1} });

  Circuit c0(bc, true), c1(bc), c2(bc);
  std::pair<size_t, size_t> cost = c0.correlationsCost();
  std::vector<Beaver::Triples> triples = this->mockTriples(3, cost.first * 2);
  std::vector<sVOLE> svoles = this->mocksVOLE(3, LABEL_SIZE, cost.second);

  BitString delta = svoles[0].delta ^ svoles[1].delta ^ svoles[2].delta;

  BitString p0 = c0.prepare(&triples[0], BitString::sample(LABEL_SIZE));
  BitString p1 = c1.prepare(&triples[1], BitString::sample(LABEL_SIZE));
  BitString p2 = c2.prepare(&triples[2], c0.freeXor ^ c1.freeXor ^ delta);

  auto sv0 = svoles[0].get(p0.size());
  auto sv1 = svoles[1].get(p1.size());
  auto sv2 = svoles[2].get(p2.size());

  // the reconstruction that the evaluator would do
  BitString gamma = p0 ^ sv0.first ^ p1 ^ sv1.first ^ p2 ^ sv2.first;
  BitString pkey = BitString::sample(LAMBDA);

  BitString ciphertexts = (
    c0.garble(pkey, gamma, sv0.second)
    ^ c1.garble(pkey, gamma, sv1.second)
    ^ c2.garble(pkey, gamma, sv2.second)
  );

  const size_t CTX_SIZE = c0.enc->ctx_size;

  ASSERT_EQ(ciphertexts.size(), CTX_SIZE * 8);

  BitString c00a = ciphertexts[{0 * CTX_SIZE, 1 * CTX_SIZE}];
  BitString c00b = ciphertexts[{1 * CTX_SIZE, 2 * CTX_SIZE}];
  BitString c01a = ciphertexts[{2 * CTX_SIZE, 3 * CTX_SIZE}];
  BitString c01b = ciphertexts[{3 * CTX_SIZE, 4 * CTX_SIZE}];
  BitString c10a = ciphertexts[{4 * CTX_SIZE, 5 * CTX_SIZE}];
  BitString c10b = ciphertexts[{5 * CTX_SIZE, 6 * CTX_SIZE}];
  BitString c11a = ciphertexts[{6 * CTX_SIZE, 7 * CTX_SIZE}];
  BitString c11b = ciphertexts[{7 * CTX_SIZE, 8 * CTX_SIZE}];

  bool mask0 = c0.masks[0] ^ c1.masks[0] ^ c0.masks[0];
  bool mask1 = c0.masks[1] ^ c1.masks[1] ^ c2.masks[1];
  bool mask2 = c0.masks[2] ^ c1.masks[2] ^ c2.masks[2];

  BitString l0 = c0.labels[0].first ^ c1.labels[0].first ^ c2.labels[0].first;
  BitString l1 = c0.labels[0].second ^ c1.labels[0].second ^ c2.labels[0].second;
  BitString r0 = c0.labels[1].first ^ c1.labels[1].first ^ c2.labels[1].first;
  BitString r1 = c0.labels[1].second ^ c1.labels[1].second ^ c2.labels[1].second;
  BitString o0 = c0.labels[2].first ^ c1.labels[2].first ^ c2.labels[2].first;
  BitString o1 = c0.labels[2].second ^ c1.labels[2].second ^ c2.labels[2].second;

  // decrypted ciphertexts
  BitString d00 = c0.enc->decrypt(l0, c00a) ^ c0.enc->decrypt(r0, c00b);
  BitString d01 = c0.enc->decrypt(l1, c01a) ^ c0.enc->decrypt(r0, c01b);
  BitString d10 = c0.enc->decrypt(l0, c10a) ^ c0.enc->decrypt(r1, c10b);
  BitString d11 = c0.enc->decrypt(l1, c11a) ^ c0.enc->decrypt(r1, c11b);

  // labels
  BitString p00 = d00[{0, LABEL_SIZE}];
  BitString p01 = d01[{0, LABEL_SIZE}];
  BitString p10 = d10[{0, LABEL_SIZE}];
  BitString p11 = d11[{0, LABEL_SIZE}];

  // indicator bits
  bool i00 = d00[LABEL_SIZE];
  bool i01 = d01[LABEL_SIZE];
  bool i10 = d10[LABEL_SIZE];
  bool i11 = d11[LABEL_SIZE];

  ASSERT_EQ(p00, ((0 ^ mask0) && (0 ^ mask1)) ^ mask2 ? o1 : o0);
  ASSERT_EQ(p01, ((1 ^ mask0) && (0 ^ mask1)) ^ mask2 ? o1 : o0);
  ASSERT_EQ(p10, ((0 ^ mask0) && (1 ^ mask1)) ^ mask2 ? o1 : o0);
  ASSERT_EQ(p11, ((1 ^ mask0) && (1 ^ mask1)) ^ mask2 ? o1 : o0);

  BitString actual(4);
  actual[0] = i00; actual[1] = i01; actual[2] = i10; actual[3] = i11;

  BitString expected(4);
  expected[0] = ((0 ^ mask0) && (0 ^ mask1)) ^ mask2;
  expected[1] = ((1 ^ mask0) && (0 ^ mask1)) ^ mask2;
  expected[2] = ((0 ^ mask0) && (1 ^ mask1)) ^ mask2;
  expected[3] = ((1 ^ mask0) && (1 ^ mask1)) ^ mask2;

  BitString reconstructed = c0.gamma[0] ^ c1.gamma[0] ^ c2.gamma[0];

  EXPECT_EQ(i00, ((0 ^ mask0) && (0 ^ mask1)) ^ mask2 ? true : false);
  EXPECT_EQ(i01, ((1 ^ mask0) && (0 ^ mask1)) ^ mask2 ? true : false);
  EXPECT_EQ(i10, ((0 ^ mask0) && (1 ^ mask1)) ^ mask2 ? true : false);
  EXPECT_EQ(i11, ((1 ^ mask0) && (1 ^ mask1)) ^ mask2 ? true : false);
}

TEST_F(CircuitTests, GarbleAES) {
  /*
  Gate g0(0, { 0, 0, 0, 1 }, { 0, 1 }, { 4 }); // w0 ^ w1 = w4
  Gate g1(1, { 0, 0, 0, 1 }, { 2, 3 }, { 5 }); // w2 & w3 = w5
  Gate g2(2, { 0, 0, 0, 1 }, { 4, 5 }, { 6 }); // w4 & w5 = w6
  BooleanCircuit bc({ g0, g1, g2 }, std::vector<int>{ 6 }, { {0, 1, 2, 3 } });
  */

  std::string bc = "../test/AES.txt";

  Circuit c0(bc, true), c1(bc), c2(bc);
  std::pair<size_t, size_t> cost = c0.correlationsCost();
  std::vector<Beaver::Triples> triples = this->mockTriples(3, cost.first * 2);
  std::vector<sVOLE> svoles = this->mocksVOLE(3, LABEL_SIZE, cost.second);

  BitString delta = svoles[0].delta ^ svoles[1].delta ^ svoles[2].delta;

  BitString p0 = c0.prepare(&triples[0], BitString::sample(LABEL_SIZE));
  BitString p1 = c1.prepare(&triples[1], BitString::sample(LABEL_SIZE));
  BitString p2 = c2.prepare(&triples[2], c0.freeXor ^ c1.freeXor ^ delta);

  auto sv0 = svoles[0].get(p0.size());
  auto sv1 = svoles[1].get(p1.size());
  auto sv2 = svoles[2].get(p2.size());

  // the reconstruction that the evaluator would do
  BitString gamma = p0 ^ sv0.first ^ p1 ^ sv1.first ^ p2 ^ sv2.first;
  BitString pkey = BitString::sample(LAMBDA);

  BitString ciphertexts = (
    c0.garble(pkey, gamma, sv0.second)
    ^ c1.garble(pkey, gamma, sv1.second)
    ^ c2.garble(pkey, gamma, sv2.second)
  );

  const size_t CTX_SIZE = c0.enc->ctx_size;

  // number of and gates in the  circuit
  size_t ands = 0;
  for (LogicGate gate : c0.gates_) {
    if (gate.type == GateType::AND) { ands++; }
  }

  ASSERT_EQ(ciphertexts.size(), ands * CTX_SIZE * 8);

  size_t i = 0;
  for (LogicGate gate : c0.gates_) {
    if (gate.type != GateType::AND) { continue; }

    // gate's garbled truth table
    BitString c00a = ciphertexts[{i + (0 * CTX_SIZE), i + (1 * CTX_SIZE)}];
    BitString c00b = ciphertexts[{i + (1 * CTX_SIZE), i + (2 * CTX_SIZE)}];
    BitString c01a = ciphertexts[{i + (2 * CTX_SIZE), i + (3 * CTX_SIZE)}];
    BitString c01b = ciphertexts[{i + (3 * CTX_SIZE), i + (4 * CTX_SIZE)}];
    BitString c10a = ciphertexts[{i + (4 * CTX_SIZE), i + (5 * CTX_SIZE)}];
    BitString c10b = ciphertexts[{i + (5 * CTX_SIZE), i + (6 * CTX_SIZE)}];
    BitString c11a = ciphertexts[{i + (6 * CTX_SIZE), i + (7 * CTX_SIZE)}];
    BitString c11b = ciphertexts[{i + (7 * CTX_SIZE), i + (8 * CTX_SIZE)}];

    // left input wire label & mask
    BitString l0 = (
      c0.labels[gate.inputs.first].first
      ^ c1.labels[gate.inputs.first].first
      ^ c2.labels[gate.inputs.first].first
    );
    BitString l1 = (
      c0.labels[gate.inputs.first].second
      ^ c1.labels[gate.inputs.first].second
      ^ c2.labels[gate.inputs.first].second
    );
    bool ml = (
      c0.masks[gate.inputs.first]
      ^ c1.masks[gate.inputs.first]
      ^ c2.masks[gate.inputs.first]
    );

    // right input wire label & mask
    BitString r0 = (
      c0.labels[gate.inputs.second].first
      ^ c1.labels[gate.inputs.second].first
      ^ c2.labels[gate.inputs.second].first
    );
    BitString r1 = (
      c0.labels[gate.inputs.second].second
      ^ c1.labels[gate.inputs.second].second
      ^ c2.labels[gate.inputs.second].second
    );
    bool mr = (
      c0.masks[gate.inputs.second]
      ^ c1.masks[gate.inputs.second]
      ^ c2.masks[gate.inputs.second]
    );

    // output wire label & mask
    BitString o0 = (
      c0.labels[gate.output].first
      ^ c1.labels[gate.output].first
      ^ c2.labels[gate.output].first
    );
    BitString o1 = (
      c0.labels[gate.output].second
      ^ c1.labels[gate.output].second
      ^ c2.labels[gate.output].second
    );
    bool mo = (
      c0.masks[gate.output]
      ^ c1.masks[gate.output]
      ^ c2.masks[gate.output]
    );

    // decrypted bitstrings
    BitString d00 = (c0.enc->decrypt(l0, c00a) ^ c0.enc->decrypt(r0, c00b));
    BitString d01 = (c0.enc->decrypt(l1, c01a) ^ c0.enc->decrypt(r0, c01b));
    BitString d10 = (c0.enc->decrypt(l0, c10a) ^ c0.enc->decrypt(r1, c10b));
    BitString d11 = (c0.enc->decrypt(l1, c11a) ^ c0.enc->decrypt(r1, c11b));

    // plaintext decrypted labels
    BitString p00 = d00[{0, LABEL_SIZE}];
    BitString p01 = d01[{0, LABEL_SIZE}];
    BitString p10 = d10[{0, LABEL_SIZE}];
    BitString p11 = d11[{0, LABEL_SIZE}];

    // check that the labels are correct
    ASSERT_EQ(p00, ((0 ^ ml) && (0 ^ mr)) ^ mo ? o1 : o0);
    ASSERT_EQ(p01, ((1 ^ ml) && (0 ^ mr)) ^ mo ? o1 : o0);
    ASSERT_EQ(p10, ((0 ^ ml) && (1 ^ mr)) ^ mo ? o1 : o0);
    ASSERT_EQ(p11, ((1 ^ ml) && (1 ^ mr)) ^ mo ? o1 : o0);

    // check that the indicator bits are correct
    ASSERT_EQ(d00[LABEL_SIZE], ((0 ^ ml) && (0 ^ mr)) ^ mo ? true : false);
    ASSERT_EQ(d01[LABEL_SIZE], ((1 ^ ml) && (0 ^ mr)) ^ mo ? true : false);
    ASSERT_EQ(d10[LABEL_SIZE], ((0 ^ ml) && (1 ^ mr)) ^ mo ? true : false);
    ASSERT_EQ(d11[LABEL_SIZE], ((1 ^ ml) && (1 ^ mr)) ^ mo ? true : false);
    i += 8 * CTX_SIZE;
  }
}

TEST_F(CircuitTests, EvaluateSmallCircuit) {
  Gate g0(0, { 0, 1, 1, 0 }, { 0, 1 }, { 4 }); // w0 ^ w1 = w4
  Gate g1(1, { 0, 0, 0, 1 }, { 2, 3 }, { 5 }); // w2 & w3 = w5
  Gate g2(2, { 0, 0, 0, 1 }, { 4, 5 }, { 6 }); // w4 & w5 = w6
  BooleanCircuit bc({ g0, g1, g2 }, std::vector<int>{ 6 }, { {0, 1, 2, 3 } });

  Circuit c0(bc, true), c1(bc), c2(bc);
  std::pair<size_t, size_t> cost = c0.correlationsCost();
  std::vector<Beaver::Triples> triples = this->mockTriples(3, cost.first * 2);
  std::vector<sVOLE> svoles = this->mocksVOLE(3, LABEL_SIZE, cost.second);

  BitString delta = svoles[0].delta ^ svoles[1].delta ^ svoles[2].delta;

  BitString p0 = c0.prepare(&triples[0], BitString::sample(LABEL_SIZE));
  BitString p1 = c1.prepare(&triples[1], BitString::sample(LABEL_SIZE));
  BitString p2 = c2.prepare(&triples[2], c0.freeXor ^ c1.freeXor ^ delta);

  auto sv0 = svoles[0].get(p0.size());
  auto sv1 = svoles[1].get(p1.size());
  auto sv2 = svoles[2].get(p2.size());

  // the reconstruction that the evaluator would do
  BitString gamma = p0 ^ sv0.first ^ p1 ^ sv1.first ^ p2 ^ sv2.first;
  BitString pkey = BitString::sample(LAMBDA);

  BitString ciphertexts = (
    c0.garble(pkey, gamma, sv0.second)
    ^ c1.garble(pkey, gamma, sv1.second)
    ^ c2.garble(pkey, gamma, sv2.second)
  );

  // input & output wire masks
  bool mask0 = c0.masks[0] ^ c1.masks[1] ^ c2.masks[0];
  bool mask1 = c0.masks[1] ^ c1.masks[1] ^ c2.masks[1];
  bool mask2 = c0.masks[2] ^ c1.masks[2] ^ c2.masks[2];
  bool mask3 = c0.masks[3] ^ c1.masks[3] ^ c2.masks[3];
  bool mask6 = c0.masks[6] ^ c1.masks[6] ^ c2.masks[6];

  // all possible inputs
  std::vector<BitString> allvalues({
    BitString("0000"), BitString("1000"), BitString("0100"), BitString("1100"),
    BitString("0010"), BitString("1010"), BitString("0110"), BitString("1110"),
    BitString("0001"), BitString("1001"), BitString("0101"), BitString("1101"),
    BitString("0011"), BitString("1011"), BitString("0111"), BitString("1111")
  });

  for (const BitString& values : allvalues) {
    // corresponding input labels
    BitString label0 = (
      !(mask0 ^ values[0])
      ? c0.labels[0].first  ^ c1.labels[0].first  ^ c2.labels[0].first
      : c0.labels[0].second ^ c1.labels[0].second ^ c2.labels[0].second
    );
    BitString label1 = (
      !(mask1 ^ values[1])
      ? c0.labels[1].first  ^ c1.labels[1].first  ^ c2.labels[1].first
      : c0.labels[1].second ^ c1.labels[1].second ^ c2.labels[1].second
    );
    BitString label2 = (
      !(mask2 ^ values[2])
      ? c0.labels[2].first  ^ c1.labels[2].first  ^ c2.labels[2].first
      : c0.labels[2].second ^ c1.labels[2].second ^ c2.labels[2].second
    );
    BitString label3 = (
      !(mask3 ^ values[3])
      ? c0.labels[3].first  ^ c1.labels[3].first  ^ c2.labels[3].first
      : c0.labels[3].second ^ c1.labels[3].second ^ c2.labels[3].second
    );

    std::map<int, std::pair<bool, BitString>> inputs;
    inputs[0] = std::make_pair(values[0] ^ mask0, label0);
    inputs[1] = std::make_pair(values[1] ^ mask1, label1);
    inputs[2] = std::make_pair(values[2] ^ mask2, label2);
    inputs[3] = std::make_pair(values[3] ^ mask3, label3);

    c0.evaluate(ciphertexts, inputs);

    bool actual = inputs[6].first ^ mask6;
    bool expected = (values[0] ^ values[1]) && (values[2] && values[3]);
    ASSERT_EQ(actual, expected);
  }
}
