#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

#include "util/bitstring.hpp"
#include "util/defines.hpp"

namespace ROT {

template<typename T>
class Base {
public:

  static const size_t DEFAULT_ELEMENT_SIZE = 128;

  Base()
    : results(std::make_shared<std::vector<T>>(0)),
      first(std::make_shared<size_t>(0)),
      last(std::make_shared<size_t>(0)) { }

  Base(std::vector<T> ots)
    : results(std::make_shared<std::vector<T>>(ots)),
      first(std::make_shared<size_t>(0)),
      last(std::make_shared<size_t>(ots.size())) { }

  // consume a single random ot with `size` bits and return it
  virtual T get(size_t size = DEFAULT_ELEMENT_SIZE) = 0;

  // number of ots remaining to be used
  size_t remaining() { return *this->last - *this->first; }

protected:
  Base(std::shared_ptr<std::vector<T>> results, size_t first, size_t last)
    : results(results), first(std::make_shared<size_t>(first)), last(std::make_shared<size_t>(last)) { }

  // actual random ot values
  std::shared_ptr<std::vector<T>> results;

  // this object's range in the results vector
  std::shared_ptr<size_t> first, last;
};

class Sender : public Base<std::pair<BitString, BitString>> {
public:
  using Base::Base;

  // generate `size` mocked random ots
  static Sender mocked(size_t size);

  std::pair<BitString, BitString> get(size_t size = DEFAULT_ELEMENT_SIZE) override;

  // use these random ots to perform some number of normal ots
  void transfer(
    std::vector<std::pair<BitString, BitString>> messages, Channel channel
  );

  // use these random ots to perform some number of normal bit-ots
  void transfer(BitString m0, BitString m1, Channel channel);

  // reserve n ots in another object (e.g., to give to a threaded process)
  Sender reserve(size_t n) {
    if (n > this->remaining()) {
      throw std::invalid_argument("[ROT::Sender::reserve] not enough ots remaining");
    }
    size_t first = *this->first;
    size_t last = *this->first + n;
    *this->first += n;

    return Sender(this->results, first, last);
  }
};

class Receiver : public Base<std::pair<bool, BitString>> {
public:
  using Base::Base;

  // generate `size` mocked random ots
  static Receiver mocked(size_t size);

  std::pair<bool, BitString> get(size_t size = DEFAULT_ELEMENT_SIZE) override;

  // use these random ots to perform some number of normal ots with the same size
  std::vector<BitString> transfer(
    BitString choices, size_t mbits, Channel channel
  );

  // use these random ots to perform some number of normal ots with different size
  std::vector<BitString> transfer(
    BitString choices, std::vector<size_t> mbits, Channel channel
  );

  // use these random ots to perform some number of normal bit-ots
  BitString transfer(BitString choices, Channel channel);

  // reserve n ots in another object (e.g., to give to a threaded process)
  Receiver reserve(size_t n) {
    if (n > this->remaining()) {
      throw std::invalid_argument("[Receiver::reserve] not enough ots remaining");
    }
    size_t first = *this->first;
    size_t last = *this->first + n;
    *this->first += n;

    return Receiver(this->results, first, last);
  }
};

std::pair<Sender, Receiver> mocked(size_t total);

}
