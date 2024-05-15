#pragma once

#include <stdexcept>

#include "util/bitstring.hpp"
#include "util/defines.hpp"

template<typename T>
class PRF {
public:
  PRF(BitString key) : key(key.toBytes()) {
    if (this->key.size() > BLOCK_SIZE) {
      throw std::invalid_argument("[PRF::PRF] provided key too large");
    }
    this->key.resize(BLOCK_SIZE);
  }

  T operator()(uint32_t x) const;
  T operator()(std::pair<uint32_t, uint32_t> x) const;

  // return a value with some bound (depends on desired type)
  T operator()(uint32_t x, uint32_t bound) const;
  T operator()(std::pair<uint32_t, uint32_t> x, uint32_t bound) const;

private:
  std::vector<unsigned char> key;

  // generic version that can be used by templated functions
  T operator()(BitString x, uint32_t bound) const;

  // aes block size
  const size_t BLOCK_SIZE = 16;
};

// simple class to sample bits without running AES for each one
class BitSampler {
public:
  BitSampler() : idx(0), cache(0) { }
  bool get() {
    if (idx == cache.size()) {
      idx = 0;
      cache = BitString::sample(LAMBDA);
    }
    return cache[idx++];
  }
private:
  size_t idx;
  BitString cache;
};

// uniformly sample a value less than `max`
uint32_t sampleLessThan(uint32_t max);

// uniformly sample `size` values less than `max`
std::vector<uint32_t> sampleVector(int size, uint32_t max);

// uniformly sample `size` distinct values less than `max`
std::vector<uint32_t> sampleDistinct(int size, uint32_t max);
