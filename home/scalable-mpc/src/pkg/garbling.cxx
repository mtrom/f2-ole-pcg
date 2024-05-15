#include "pkg/garbling.hpp"

#include "pkg/enc.hpp"
#include "util/defines.hpp"
#include "util/random.hpp"

BitString Evaluator::run(Circuit circuit, const BitString& input) {
  // sample a public key for the encryption scheme
  BitString pkey = BitString::sample(LAMBDA);

  // our shares of 𝛾 for each AND gate
  BitString gamma = circuit.prepare(&this->triples, this->svoles.delta);

  // mask with our share of sVOLE correlations
  auto correlations = this->svoles.get(gamma.size());
  gamma ^= correlations.first;

  // read all garbler's shares & reconstruct
  BitString inc(gamma.size());
  for (Channel channel : this->channels) {
    channel->read(inc.data(), inc.nBytes());
    gamma ^= inc;
  }

  // send reconstruction & public key to all garblers
  BitString out = pkey + gamma;
  for (Channel channel : channels) {
    channel->write(pkey.data(), pkey.nBytes());
    channel->write(gamma.data(), gamma.nBytes());
  }

  // do the actual garbling
  BitString ciphertexts = circuit.garble(pkey, gamma, correlations.second);

  // get the masks associated with various inputs
  BitString indicators = circuit.getInputMasks(0, channels.size() + 1, input);
  BitString outmasks = circuit.getOutputMasks();

  // reconstruct the ciphertexts & input indicator bits
  {
    BitString buffer0(ciphertexts.size());
    BitString buffer1(indicators.size());
    BitString buffer2(outmasks.size());
    for (Channel channel : channels) {
      channel->read(buffer0.data(), buffer0.nBytes());
      channel->read(buffer1.data(), buffer1.nBytes());
      channel->read(buffer2.data(), buffer2.nBytes());
      ciphertexts ^= buffer0;
      indicators  ^= buffer1;
      outmasks    ^= buffer2;
    }
  }

  // send back the input indicator bits
  for (Channel channel : channels) {
    channel->write(indicators.data(), indicators.nBytes());
  }

  // reconstruct the labels
  BitString labels = circuit.getInputLabels(indicators);
  {
    BitString buffer(labels.size());
    for (Channel channel : channels) {
      channel->read(buffer.data(), buffer.nBytes());
      labels ^= buffer;
    }
  }

  // collect the input indicator bits & labels
  std::map<int, std::pair<bool, BitString>> wiremap;
  std::vector<int> inwires = circuit.inputs();
  for (size_t i = 0; i < inwires.size(); i++) {
    wiremap[inwires[i]] = std::make_pair(
      indicators[i],
      labels[{i * LABEL_SIZE, (i + 1) * LABEL_SIZE}]
    );
  }

  circuit.evaluate(ciphertexts, wiremap);

  // collect the output indicator bits
  std::vector<int> outwires = circuit.outputs();
  BitString output(outwires.size());
  for (size_t i = 0; i < outwires.size(); i++) {
    output[i] = wiremap[outwires[i]].first ^ outmasks[i];
  }

  return output;
}

void Garbler::run(Circuit circuit, const BitString& input) {
  // our shares of 𝛾 for each AND gate
  BitString gamma = circuit.prepare(&this->triples, this->svoles.delta);

  // mask with our share of sVOLE correlations
  auto correlations = this->svoles.get(gamma.size());
  gamma ^= correlations.first;

  // send to evaluator to reconstruct
  channel->write(gamma.data(), gamma.nBytes());

  // receive the reconstruction & a public key for lpn encryption
  BitString pkey(LAMBDA);
  channel->read(pkey.data(), pkey.nBytes());
  channel->read(gamma.data(), gamma.nBytes());

  // do the actual garbling
  BitString ciphertexts = circuit.garble(pkey, gamma, correlations.second);

  // get the masks associated with various inputs
  BitString inmasks  = circuit.getInputMasks(this->id, this->parties, input);
  BitString outmasks = circuit.getOutputMasks();

  // send to evaluator to reconstruct
  channel->write(ciphertexts.data(), ciphertexts.nBytes());
  channel->write(inmasks.data(), inmasks.nBytes());
  channel->write(outmasks.data(), inmasks.nBytes());
  channel->read(inmasks.data(), inmasks.nBytes());

  // send out the labels
  BitString labels = circuit.getInputLabels(inmasks);
  channel->write(labels.data(), labels.nBytes());
}
