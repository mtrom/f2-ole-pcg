#pragma once

#include "util/random.hpp"

class sVOLE {
public:
  sVOLE() : delta(0), s(0), u(0), total_(0) { }
  sVOLE(BitString delta, BitString s, std::vector<BitString> u)
    : delta(delta), s(s), u(u), total_(s.size()) { }

  // consume one correlation
  std::pair<bool, BitString> get() {
    if (remaining() == 0) {
      throw std::out_of_range("[sVOLE::get] out of correlations");
    }
    used++;
    return std::make_pair(s[used - 1], u[used - 1]);
  }

  // consume multiple correlation
  std::pair<BitString, std::vector<BitString>> get(size_t n) {
    if (remaining() < n) {
      throw std::out_of_range("[sVOLE::get(size_t)] out of correlations");
    }
    auto out = std::make_pair(
      s[{used, used + n}],
      std::vector<BitString>(u.begin() + used, u.begin() + used + n)
    );
    used += n;
    return out;
  }

  // reserve multiple correlations
  sVOLE reserve(size_t n) {
    if (remaining() < n) {
      throw std::out_of_range("[sVOLE::get] out of correlations");
    }
    sVOLE out(
      delta,
      s[{used, used + n}],
      std::vector<BitString>(u.begin() + used, u.begin() + used + n)
    );
    used += n;
    return out;
  }

  // total number of correlations in this object
  size_t total() const { return total_; }

  // number of correlations remaining to be used
  size_t remaining() const { return total_ - used; }

  const BitString delta;
protected:
  const BitString s;
  const std::vector<BitString> u;

  size_t total_;
  size_t used = 0;
};
