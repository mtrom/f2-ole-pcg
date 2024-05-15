#include "pkg/rot.hpp"

#include <stdexcept>

#include "libscapi/include/interactive_mid_protocols/OTBatch.hpp"
#include "libscapi/include/interactive_mid_protocols/OTExtensionBristol.hpp"

////////////////////////////////////////////////////////////////////////////////
// GET
////////////////////////////////////////////////////////////////////////////////

std::pair<BitString, BitString> RandomOTSender::get(size_t size) {
  if (results.empty()) {
    throw std::out_of_range("[RandomOTSender::get] out of random ots");
  }
  BitString m0, m1;
  std::tie(m0, m1) = results.back();
  results.pop_back();

  if (size < RandomOT::DEFAULT_ELEMENT_SIZE) {
    m0 = m0[{0, size}];
    m1 = m1[{0, size}];
  } else if (size > RandomOT::DEFAULT_ELEMENT_SIZE) {
    m0 = m0.aes(size);
    m1 = m1.aes(size);
  }

  return std::make_pair(m0, m1);
}

std::pair<bool, BitString> RandomOTReceiver::get(size_t size) {
  if (results.empty()) {
    throw std::out_of_range("[RandomOTReceiver::get] out of random ots");
  }
  bool b;
  BitString mb;
  std::tie(b, mb) = results.back();
  results.pop_back();

  if (size < RandomOT::DEFAULT_ELEMENT_SIZE) {
    mb = mb[{0, size}];
  } else if (size > RandomOT::DEFAULT_ELEMENT_SIZE) {
    mb = mb.aes(size);
  }

  return std::make_pair(b, mb);
}

////////////////////////////////////////////////////////////////////////////////
// RUN
////////////////////////////////////////////////////////////////////////////////

void RandomOTSender::run(size_t total, std::shared_ptr<CommParty> channel, int port) {
  OTExtensionRandomizedSInput input(total, RandomOT::DEFAULT_ELEMENT_SIZE);
  OTExtensionBristolSender ot(port, true, channel);
  shared_ptr<OTBatchSOutput> output = ot.transfer(&input);

  vector<byte> m0 = ((OTExtensionRandomizedSOutput*) output.get())->getR0Arr();
  vector<byte> m1 = ((OTExtensionRandomizedSOutput*) output.get())->getR1Arr();

  for (size_t i = 0; i < m0.size(); i += RandomOT::DEFAULT_ELEMENT_SIZE / 8) {
    this->results.push_back(
      std::make_pair(
        BitString(m0.data() + i, RandomOT::DEFAULT_ELEMENT_SIZE),
        BitString(m1.data() + i, RandomOT::DEFAULT_ELEMENT_SIZE)
      )
    );
  }
  this->total_ = total;
}

void RandomOTReceiver::run(size_t total, std::shared_ptr<CommParty> channel, int port) {
  BitString b = BitString::sample(total);
  OTExtensionRandomizedRInput input(b.expand(), RandomOT::DEFAULT_ELEMENT_SIZE);
  OTExtensionBristolReceiver ot("localhost", port, true, channel);

  shared_ptr<OTBatchROutput> output = ot.transfer(&input);
  std::vector<byte> mb = ((OTOnByteArrayROutput*) output.get())->getXSigma();

  for (size_t i = 0, j = 0; j < mb.size(); i++, j += RandomOT::DEFAULT_ELEMENT_SIZE / 8) {
    this->results.push_back(
      std::make_pair(b[i], BitString(mb.data() + j, RandomOT::DEFAULT_ELEMENT_SIZE))
    );
  }
  this->total_ = total;
}

////////////////////////////////////////////////////////////////////////////////
// TRANSFER (MULTI-BIT MESSAGES)
////////////////////////////////////////////////////////////////////////////////

void RandomOTSender::transfer(
  std::vector<std::pair<BitString, BitString>> messages,
  std::shared_ptr<CommParty> channel
) {
  BitString swap(messages.size());
  channel->read(swap.data(), swap.nBytes());

  BitString outgoing;
  for (size_t i = 0; i < messages.size(); i++) {
    BitString m0, m1, r0, r1;
    std::tie(m0, m1) = messages[i];
    std::tie(r0, r1) = this->get(m0.size());

    m0 ^= swap[i] ? r1 : r0;
    m1 ^= swap[i] ? r0 : r1;
    outgoing += m0;
    outgoing += m1;
  }

  channel->write(outgoing.data(), outgoing.nBytes());
}

std::vector<BitString> RandomOTReceiver::transfer(
    BitString choices,
    size_t mbits,
    std::shared_ptr<CommParty> channel
) {
  std::vector<std::pair<bool, BitString>> reserved(choices.size());
  BitString swap(choices.size());
  for (size_t i = 0; i < choices.size(); i++) {
    reserved[i] = this->get(mbits);
    swap[i] = reserved[i].first ^ choices[i];
  }
  channel->write(swap.data(), swap.nBytes());

  BitString incoming(mbits * 2 * choices.size());
  channel->read(incoming.data(), incoming.nBytes());

  std::vector<BitString> messages;
  for (size_t i = 0, j = 0; i < choices.size(); i++, j += 2 * mbits) {
    size_t idx = choices[i] ? j + mbits : j;
    BitString mb = incoming[{idx, idx + mbits}];

    mb ^= reserved[i].second;
    messages.push_back(mb);
  }
  return messages;
}

std::vector<BitString> RandomOTReceiver::transfer(
    BitString choices,
    std::vector<size_t> mbits,
    std::shared_ptr<CommParty> channel
) {
  std::vector<std::pair<bool, BitString>> reserved(choices.size());
  BitString swap(choices.size());
  size_t total_size = 0;
  for (size_t i = 0; i < choices.size(); i++) {
    reserved[i] = this->get(mbits[i]);
    swap[i] = reserved[i].first ^ choices[i];
    total_size += mbits[i];
  }
  channel->write(swap.data(), swap.nBytes());

  BitString incoming(total_size * 2);
  channel->read(incoming.data(), incoming.nBytes());

  std::vector<BitString> messages;
  for (size_t i = 0, j = 0; i < choices.size(); j += 2 * mbits[i], i++) {
    size_t idx = choices[i] ? j + mbits[i] : j;
    BitString mb = incoming[{idx, idx + mbits[i]}];

    mb ^= reserved[i].second;
    messages.push_back(mb);
  }
  return messages;
}

////////////////////////////////////////////////////////////////////////////////
// TRANSFER (SINGLE-BIT MESSAGES)
////////////////////////////////////////////////////////////////////////////////

void RandomOTSender::transfer(BitString m0, BitString m1, std::shared_ptr<CommParty> channel) {
  BitString swap(m0.size());
  channel->read(swap.data(), swap.nBytes());

  for (size_t i = 0; i < m0.size(); i++) {
    auto rbs = this->get(1);
    bool r0 = rbs.first[0], r1 = rbs.second[0];

    m0[i] ^= swap[i] ? r1 : r0;
    m1[i] ^= swap[i] ? r0 : r1;
  }

  BitString outgoing = m0 + m1;
  channel->write(outgoing.data(), outgoing.nBytes());
}

BitString RandomOTReceiver::transfer(
    BitString choices,
    std::shared_ptr<CommParty> channel
) {
  std::vector<std::pair<bool, BitString>> reserved(choices.size());
  BitString swap(choices.size());
  for (size_t i = 0; i < choices.size(); i++) {
    reserved[i] = this->get(1);
    swap[i] = reserved[i].first ^ choices[i];
  }
  channel->write(swap.data(), swap.nBytes());

  BitString messages(choices.size() * 2);
  channel->read(messages.data(), messages.nBytes());

  BitString m0 = messages[{0, choices.size()}];
  BitString m1 = messages[{choices.size(), choices.size() * 2}];

  BitString out(choices.size());
  for (size_t i = 0; i < choices.size(); i++) {
    out[i] = choices[i] ? m1[i] : m0[i];
    out[i] ^= reserved[i].second[0];
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////

std::pair<RandomOTSender, RandomOTReceiver> mockRandomOT(size_t total) {
  std::vector<std::pair<BitString, BitString>> sender;
  std::vector<std::pair<bool, BitString>> receiver;

  BitString b = BitString::sample(total);
  for (size_t i = 0; i < total; i++) {
    BitString m0 = BitString::sample(RandomOTSender::DEFAULT_ELEMENT_SIZE);
    BitString m1 = BitString::sample(RandomOTSender::DEFAULT_ELEMENT_SIZE);
    BitString mb = b[i] ? m1 : m0;

    sender.push_back(std::make_pair(m0, m1));
    receiver.push_back(std::make_pair(b[i], mb));
  }

  return std::make_pair(RandomOTSender(sender), RandomOTReceiver(receiver));
}
