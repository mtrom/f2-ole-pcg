#pragma once

#include <algorithm>
#include <libscapi/include/comm/Comm.hpp>
#include <stdexcept>
#include <vector>

#include "util/bitstring.hpp"

template<typename T>
class RandomOT {
public:

  static const size_t DEFAULT_ELEMENT_SIZE = 128;

  RandomOT() : total_(0), results(0) { }
  RandomOT(std::vector<T> ots) : total_(ots.size()), results(ots) { }

  // run the protocol to actually get the random ots
  virtual void run(size_t total, std::shared_ptr<CommParty> channel, int port) = 0;

  // consume a single random ot with `size` bits and return it
  virtual T get(size_t size = DEFAULT_ELEMENT_SIZE) = 0;

  // total number of ots in this object
  size_t total() { return total_; }

  // number of ots remaining to be used
  size_t remaining() { return results.size(); }

protected:
  // total number requested in run()
  size_t total_;

  // remaining ot output
  std::vector<T> results;

  // used by subclasses
  std::vector<T> reserve_(size_t n) {
    if (n > this->remaining()) {
      throw std::invalid_argument("[RandomOT::get] not enough ots remaining");
    }

    std::vector<T> out;
    out.reserve(n);

    // copy last `n` over to `out`
    std::copy(results.end() - n, results.end(), std::back_inserter(out));
    results.erase(results.end() - n, results.end());

    return out;
  }
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
    return RandomOTSender(RandomOT<std::pair<BitString, BitString>>::reserve_(n));
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
    return RandomOTReceiver(RandomOT<std::pair<bool, BitString>>::reserve_(n));
  }
};

// for testing purposes
std::pair<RandomOTSender, RandomOTReceiver> mockRandomOT(size_t total);
