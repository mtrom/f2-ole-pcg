#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

#include <libscapi/include/comm/Comm.hpp>

#include "util/bitstring.hpp"

template<typename T>
class RandomOT {
public:

  static const size_t DEFAULT_ELEMENT_SIZE = 128;

  RandomOT()
    : results(std::make_shared<std::vector<T>>(0)),
      first(std::make_shared<size_t>(0)),
      last(std::make_shared<size_t>(0)) { }

  RandomOT(std::vector<T> ots)
    : results(std::make_shared<std::vector<T>>(ots)),
      first(std::make_shared<size_t>(0)),
      last(std::make_shared<size_t>(ots.size())) { }

  // run the protocol to actually get the random ots
  virtual void run(size_t total, std::shared_ptr<CommParty> channel, int port) = 0;

  // consume a single random ot with `size` bits and return it
  virtual T get(size_t size = DEFAULT_ELEMENT_SIZE) = 0;

  // number of ots remaining to be used
  size_t remaining() { return *this->last - *this->first; }

protected:
  RandomOT(std::shared_ptr<std::vector<T>> results, size_t first, size_t last)
    : results(results), first(std::make_shared<size_t>(first)), last(std::make_shared<size_t>(last)) { }

  // actual random ot values
  std::shared_ptr<std::vector<T>> results;

  // this object's range in the results vector
  std::shared_ptr<size_t> first, last;
};

class RandomOTSender : public RandomOT<std::pair<BitString, BitString>> {
public:
  using RandomOT::RandomOT;

  void run(size_t total, std::shared_ptr<CommParty> channel, int port) override;
  std::pair<BitString, BitString> get(size_t size = DEFAULT_ELEMENT_SIZE) override;

  // use these random ots to perform some number of normal ots
  void transfer(
    std::vector<std::pair<BitString, BitString>> messages,
    std::shared_ptr<CommParty> channel
  );

  // use these random ots to perform some number of normal bit-ots
  void transfer(BitString m0, BitString m1, std::shared_ptr<CommParty> channel);

  // reserve n ots in another object (e.g., to give to a threaded process)
  RandomOTSender reserve(size_t n) {
    if (n > this->remaining()) {
      throw std::invalid_argument("[RandomOTSender::reserve] not enough ots remaining");
    }
    size_t first = *this->first;
    size_t last = *this->first + n;
    *this->first += n;

    return RandomOTSender(this->results, first, last);
  }
};

class RandomOTReceiver : public RandomOT<std::pair<bool, BitString>> {
public:
  using RandomOT::RandomOT;

  void run(size_t total, std::shared_ptr<CommParty> channel, int port) override;
  std::pair<bool, BitString> get(size_t size = DEFAULT_ELEMENT_SIZE) override;

  // use these random ots to perform some number of normal ots with the same size
  std::vector<BitString> transfer(
      BitString choices,
      size_t mbits,
      std::shared_ptr<CommParty> channel
  );

  // use these random ots to perform some number of normal ots with different size
  std::vector<BitString> transfer(
      BitString choices,
      std::vector<size_t> mbits,
      std::shared_ptr<CommParty> channel
  );

  // use these random ots to perform some number of normal bit-ots
  BitString transfer(BitString choices, std::shared_ptr<CommParty> channel);

  // reserve n ots in another object (e.g., to give to a threaded process)
  RandomOTReceiver reserve(size_t n) {
    if (n > this->remaining()) {
      throw std::invalid_argument("[RandomOTReceiver::reserve] not enough ots remaining");
    }
    size_t first = *this->first;
    size_t last = *this->first + n;
    *this->first += n;

    return RandomOTReceiver(this->results, first, last);
  }
};

// for testing purposes
std::pair<RandomOTSender, RandomOTReceiver> mockRandomOT(size_t total);
