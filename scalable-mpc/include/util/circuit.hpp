#pragma once

#include <boost/range/adaptor/reversed.hpp>
#include <libscapi/include/circuits/BooleanCircuits.hpp>

#include "pkg/pcg.hpp"
#include "pkg/enc.hpp"
#include "util/params.hpp"

enum GateType { AND, XOR };

class LogicGate {
public:
  LogicGate(int id, GateType type, std::pair<int, int> inputs, int output)
    : id(id), type(type), inputs(inputs), output(output) { }

  const int id;
  const GateType type;
  const std::pair<int, int> inputs;
  const int output;
};

class Circuit {
protected:
  std::vector<int> inputs_;
  std::vector<int> outputs_;
  std::vector<LogicGate> gates_;

  // wire labels & masks
  std::map<int, std::pair<BitString, BitString>> labels;
  std::map<int, bool> masks;
  BitString freeXor; // (k0 ^ k1) for all wires

  // maps gate id to our share of 𝛾 = g(α ^ rᵤ, β ^ rᵥ) ^ rₒ ∀ α,β
  std::map<int, BitString> gamma;

  // true if this is the evaluator party
  const bool evaluator;

  // n-party correlations used in garbling & encrypting
  Beaver::Triples* triples;

  // doubly-linear homomophic encryption scheme
  std::unique_ptr<LPN::Encryptor> enc;
public:
  // read in from file
  Circuit(std::string filename, bool evaluator = false)
    : Circuit(BooleanCircuit(new scannerpp::File(filename)), evaluator) { }

  // convert a libscapi::BooleanCircuit to our circuit
  Circuit(const BooleanCircuit& bc, bool evaluator = false);

  // set wire labels & masks; return online garbling message
  BitString prepare(Beaver::Triples* triples, const BitString& freeXor);

  // compute & return gate ciphertexts
  BitString garble(
    const BitString& pkey, const BitString& gamma, const std::vector<BitString>& shares
  );

  // evaluate the circuit with the given ciphertexts
  void evaluate(
    const BitString& ciphertexts,
    std::map<int, std::pair<bool, BitString>>& inputs
  ) const;

  // how many triples & svoles (respectively) it takes to garble this circuit
  std::pair<size_t, size_t> correlationsCost() const;

  // get the ids for the input / output wires
  std::vector<int> inputs() const { return inputs_; }
  std::vector<int> outputs() const { return outputs_; }

  // for input & output processing
  BitString getInputMasks(uint16_t pid, uint16_t parties, BitString values) const;
  BitString getInputLabels(const BitString& indicators) const;
  BitString getOutputMasks() const;

  // iterate through the gates topologically or reverse topologically
  const std::vector<LogicGate>& getGates() const { return gates_; }
  auto getGatesReversed() -> decltype(boost::adaptors::reverse(this->gates_)) {
    return boost::adaptors::reverse(gates_);
  }
};

