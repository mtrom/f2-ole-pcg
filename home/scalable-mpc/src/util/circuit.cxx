#include "util/circuit.hpp"

#include "util/bitstring.hpp"

std::pair<size_t, size_t> Circuit::correlationsCost() const {
  size_t ands = 0;
  for (LogicGate gate : this->gates_) {
    if (gate.type == GateType::AND) { ands++; }
  }

  return std::make_pair(
    ands + (ands * 8 * ECC::CODEWORD_SIZE(LABEL_SIZE + 1) / 4),
    4 * ands
  );
}

Circuit::Circuit(const BooleanCircuit& bc, bool evaluator) : evaluator(evaluator) {
  for (Gate gate : bc.getGates()) {
    std::vector<int> inputs = gate.getInputWireIndices();
    std::vector<int> output = gate.getOutputWireIndices();
    std::vector<bool> tt = gate.getTruthTable();
    if (tt == vector<bool>{false, false, false, true}) {
      this->gates_.push_back(
        LogicGate(
          gate.getGateNumber(), GateType::AND, std::make_pair(inputs[0], inputs[1]), output[0]
        )
      );
    } else if (tt == vector<bool>{false, true, true, false}) {
      this->gates_.push_back(
        LogicGate(
          gate.getGateNumber(), GateType::XOR, std::make_pair(inputs[0], inputs[1]), output[0]
        )
      );
    } else if (tt == vector<bool>{true, false}) {
      this->gates_.push_back(
        LogicGate(
          gate.getGateNumber(), GateType::XOR, std::make_pair(inputs[0], -1), output[0]
        )
      );
      if (std::find(inputs.begin(), inputs.end(), -1) == inputs.end()) {
        this->inputs_.push_back(-1);
      }
    } else {
      BitString table(tt.size());
      for (size_t i = 0; i < tt.size(); i++) {
        table[i] = tt[i];
      }
      throw std::invalid_argument(
        "[Circuit::Circuit] invalid truth table found: " + table.toString()
      );
    }
  }

  for (std::vector<int> outputs : bc.eachPartysOutputWires) {
    for (int wire : outputs) {
      this->outputs_.push_back(wire);
    }
  }

  for (std::vector<int> inputs : bc.eachPartysInputWires) {
    for (int wire : inputs) {
      this->inputs_.push_back(wire);
    }
  }
}

BitString Circuit::prepare(Beaver::Triples* triples, const BitString& freeXor) {
  this->triples = triples;
  this->freeXor = freeXor;
  BitSampler sampler;

  // input wire labels
  for (int wire : this->inputs_) {
    BitString label = BitString::sample(LABEL_SIZE);
    this->labels[wire] = std::make_pair(label, label ^ freeXor);
  }

  // set all other wire labels
  for (LogicGate gate : this->gates_) {
    BitString label = (
      gate.type == GateType::XOR ?
      this->labels[gate.inputs.first].first ^ this->labels[gate.inputs.second].first :
      BitString::sample(LABEL_SIZE)
    );

    this->labels[gate.output] = std::make_pair(label, label ^ freeXor);
  }

  // output wire masks
  for (int wire : this->outputs_) {
    this->masks[wire] = sampler.get();
  }

  // set all other wire masks (and compute secret shares of g(α ^ rᵤ, β ^ rᵥ) ^ rₒ)
  for (LogicGate gate : boost::adaptors::reverse(this->gates_)) {
    if (gate.type == GateType::XOR) {
      this->masks[gate.inputs.first] = sampler.get();
      this->masks[gate.inputs.second] = this->masks[gate.inputs.first] ^ this->masks[gate.output];
    } else {
      Beaver::Triple t = triples->get();
      this->masks[gate.inputs.first] = t.a;
      this->masks[gate.inputs.second] = t.b;

      this->gamma[gate.id] = BitString(4);
      this->gamma[gate.id][0] = t.c ^ this->masks[gate.output];
      this->gamma[gate.id][1] = t.b ^ t.c ^ this->masks[gate.output];
      this->gamma[gate.id][2] = t.a ^ t.c ^ this->masks[gate.output];
      this->gamma[gate.id][3] = (evaluator ? 1 : 0) ^ t.a ^ t.b ^ t.c ^ this->masks[gate.output];
    }
  }

  // serialize gamma into a message (with AND gates in topo-order)
  BitString msg;
  for (LogicGate gate : this->gates_) {
    if (gate.type != GateType::AND) { continue; }
    if (this->gamma.find(gate.id) == this->gamma.end()) {
      throw std::logic_error("[Circuit::prepare] gate not found in map");
    }

    msg += this->gamma[gate.id];
  }

  return msg;
}

BitString Circuit::garble(
  const BitString& pkey, const BitString& reconstructed, const std::vector<BitString>& shares
) {
  // instantiate the lpn encryption
  this->enc = std::make_unique<LPN::Encryptor>(
    EncryptionParams(pkey, LABEL_SIZE, LABEL_SIZE + 1), this->triples, this->evaluator
  );

  unsigned char i = 0;
  BitString out;
  for (LogicGate gate : this->gates_) {
    if (gate.type != GateType::AND) { continue; }

    do {
      // share of truth table output
      BitString outkey = (
        reconstructed[i] ?
        labels[gate.output].first ^ shares[i] ^ this->freeXor :
        labels[gate.output].first ^ shares[i]
      );

      // split the output label & wire into two shares
      BitString keyshare0 = BitString::sample(outkey.size() + 1);
      BitString keyshare1(outkey.data(), outkey.size() + 1);
      keyshare1[outkey.size()] = this->gamma[gate.id][i % 4];
      keyshare1 ^= keyshare0;

      // encrypt one share with the first input's key
      out += this->enc->encrypt(
        (i % 2) == 0 ? labels[gate.inputs.first].first : labels[gate.inputs.first].second,
        keyshare0
      );

      // encrypt the other share with the second input's key
      out += this->enc->encrypt(
        (i % 4) < 2 ? labels[gate.inputs.second].first : labels[gate.inputs.second].second,
        keyshare1
      );
      i++;
    } while (i % 4 != 0);
  }
  return out;
}

void Circuit::evaluate(
  const BitString& ciphertexts,
  std::map<int, std::pair<bool, BitString>>& inputs
) const {
  if (!evaluator) { throw std::logic_error("[Circuit::evaluate] not an evaluator"); }

  // position in `ciphertexts`
  size_t i = 0;
  for (LogicGate gate : this->gates_) {
    bool pubu, pubv;
    BitString labelu, labelv;
    std::tie(pubu, labelu) = inputs[gate.inputs.first];
    std::tie(pubv, labelv) = inputs[gate.inputs.second];

    if (gate.type == XOR) {
      inputs[gate.output] = std::make_pair(pubu ^ pubv, labelu ^ labelv);
      continue;
    }
    size_t offset = 0;
    if      ( pubu && !pubv) { offset = 2 * this->enc->ctx_size; }
    else if (!pubu &&  pubv) { offset = 4 * this->enc->ctx_size; }
    else if ( pubu &&  pubv) { offset = 6 * this->enc->ctx_size; }

    BitString decrypted = this->enc->decrypt(
      labelu, ciphertexts[{i + offset, i + offset + this->enc->ctx_size}]
    ) ^ this->enc->decrypt(
      labelv, ciphertexts[{i + offset + enc->ctx_size, i + offset + (2 * enc->ctx_size)}]
    );

    inputs[gate.output] = std::make_pair(decrypted[LABEL_SIZE], decrypted[{0, LABEL_SIZE}]);
    i += 8 * this->enc->ctx_size;
  }
}

BitString Circuit::getInputMasks(uint16_t pid, uint16_t parties, BitString values) const {
  BitString out(this->inputs_.size());
  for (size_t i = 0, v = 0; i < this->inputs_.size(); i++) {
    out[i] = this->masks.at(this->inputs_[i]);

    // arbitrarily hand out input wires based on id
    if (this->inputs_[i] != -1 && i % parties == pid) {
      out[i] ^= values[v];
      v++;
    }

    // p0 provides the constant input for NOT gates
    else if (this->inputs_[i] == -1 && pid == 0) {
      out[i] ^= 1;
    }
  }

  return out;
}

BitString Circuit::getInputLabels(const BitString& indicators) const {
  BitString out;
  for (size_t i = 0; i < indicators.size(); i++) {
    if (indicators[i]) {
      out += this->labels.at(this->inputs_[i]).second;
    } else {
      out += this->labels.at(this->inputs_[i]).first;
    }
  }
  return out;
}

BitString Circuit::getOutputMasks() const {
  BitString out(this->outputs_.size());
  for (size_t i = 0, v = 0; i < this->outputs_.size(); i++) {
    out[i] = this->masks.at(this->outputs_[i]);
  }
  return out;
}
