#include "pkg/rot.hpp"

#include <stdexcept>

#include "util/random.hpp"

namespace ROT {

////////////////////////////////////////////////////////////////////////////////
// GET
////////////////////////////////////////////////////////////////////////////////

std::pair<BitString, BitString> Sender::get(size_t size) {
  if (*this->first == *this->last) {
    throw std::out_of_range("[Sender::get] out of random ots");
  }
  BitString m0, m1;
  std::tie(m0, m1) = results->at(*this->first);
  (*this->first)++;

  if (size < Base::DEFAULT_ELEMENT_SIZE) {
    m0 = m0[{0, size}];
    m1 = m1[{0, size}];
  } else if (size > Base::DEFAULT_ELEMENT_SIZE) {
    m0 = m0.aes(size);
    m1 = m1.aes(size);
  }

  return std::make_pair(m0, m1);
}

std::pair<bool, BitString> Receiver::get(size_t size) {
  if (*this->first == *this->last) {
    throw std::out_of_range("[Receiver::get] out of random ots");
  }
  bool b;
  BitString mb;
  std::tie(b, mb) = results->at(*this->first);
  (*this->first)++;

  if (size < Base::DEFAULT_ELEMENT_SIZE) {
    mb = mb[{0, size}];
  } else if (size > Base::DEFAULT_ELEMENT_SIZE) {
    mb = mb.aes(size);
  }

  return std::make_pair(b, mb);
}

////////////////////////////////////////////////////////////////////////////////
// TRANSFER (MULTI-BIT MESSAGES)
////////////////////////////////////////////////////////////////////////////////

void Sender::transfer(
  std::vector<std::pair<BitString, BitString>> messages,
  Channel channel
) {
  if (messages.size() > this->remaining()) {
    throw std::runtime_error("[Sender::transfer(std::vector, ...)] out of random ots");
  }

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

std::vector<BitString> Receiver::transfer(
  BitString choices, size_t mbits, Channel channel
) {
  if (choices.size() > this->remaining()) {
    throw std::runtime_error(
      "[Receiver::transfer(BitString, size_t, ...)] out of random ots"
    );
  }
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

std::vector<BitString> Receiver::transfer(
  BitString choices, std::vector<size_t> mbits, Channel channel
) {
  if (choices.size() > this->remaining()) {
    throw std::runtime_error(
      "[Receiver::transfer(BitString, std::vector<size_t>, ...)] out of random ots"
    );
  }

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

void Sender::transfer(BitString m0, BitString m1, Channel channel) {
  if (m0.size() > this->remaining()) {
    throw std::runtime_error(
      "[Sender::transfer(BitString, BitString, ...)] out of random ots"
    );
  }

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

BitString Receiver::transfer(BitString choices, Channel channel) {
  if (choices.size() > this->remaining()) {
    throw std::runtime_error(
      "[Receiver::transfer(BitString, ...)] out of random ots"
    );
  }

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

// prf key to allow for mock rots
const BitString MOCKING_KEY = BitString(
  "0000111100001111000011110000111100001111000011110000111100001111"
  "0000111100001111000011110000111100001111000011110000111100001111"
);

Sender Sender::mocked(size_t size) {
  std::vector<std::pair<BitString, BitString>> messages;
  PRF<BitString> prf(MOCKING_KEY);

  for (size_t i = 0; i < 2 * size; i += 2) {
    BitString m0 = prf(i, Sender::DEFAULT_ELEMENT_SIZE);
    BitString m1 = prf(i + 1, Sender::DEFAULT_ELEMENT_SIZE);
    messages.push_back(std::make_pair(m0, m1));
  }
  return Sender(messages);
}

Receiver Receiver::mocked(size_t size) {
  std::vector<std::pair<bool, BitString>> messages;
  PRF<BitString> prf(MOCKING_KEY);

  BitString b = BitString::sample(size);
  for (size_t i = 0; i < 2 * size; i += 2) {
    BitString mb = prf((b[i / 2] ? i + 1 : i),  Sender::DEFAULT_ELEMENT_SIZE);
    messages.push_back(std::make_pair(b[i / 2], mb));
  }
  return Receiver(messages);
}


std::pair<Sender, Receiver> mocked(size_t total) {
  std::vector<std::pair<BitString, BitString>> sender;
  std::vector<std::pair<bool, BitString>> receiver;

  BitString b = BitString::sample(total);
  for (size_t i = 0; i < total; i++) {
    BitString m0 = BitString::sample(Sender::DEFAULT_ELEMENT_SIZE);
    BitString m1 = BitString::sample(Sender::DEFAULT_ELEMENT_SIZE);
    BitString mb = b[i] ? m1 : m0;

    sender.push_back(std::make_pair(m0, m1));
    receiver.push_back(std::make_pair(b[i], mb));
  }

  return std::make_pair(Sender(sender), Receiver(receiver));
}

}
