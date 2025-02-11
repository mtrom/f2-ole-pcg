#pragma once

#include <cstdlib>
#include <mutex>
#include <stdexcept>

#include <openssl/evp.h>
#include <openssl/provider.h>

#include <cryptoTools/Common/block.h>
#include <cryptoTools/Crypto/AES.h>

#include "util/bitstring.hpp"
#include "util/defines.hpp"

/**
 * depending on the performance impact, this either uses openssl's aes hooks
 * or cryptoTools's PRGN
 */
template<typename T>
class PRF {
public:
  PRF(BitString key) : aes(osuCrypto::toBlock(key.data())) { this->setKey(key.toBytes()); }

  T operator()(uint32_t x) const;
  T operator()(std::pair<uint32_t, uint32_t> x) const;

  // return a value with some bound (depends on desired type)
  T operator()(uint32_t x, uint32_t bound) const;
  T operator()(std::pair<uint32_t, uint32_t> x, uint32_t bound) const;

  void setKey(const std::vector<unsigned char>& key) {
    if (key.size() > BLOCK_SIZE) {
      throw std::invalid_argument("[PRF::setKey] provided key too large");
    }
    this->key = key;
    this->key.resize(BLOCK_SIZE);
  }
  const std::vector<unsigned char> getKey() const { return key; }

private:
  std::vector<unsigned char> key;

  osuCrypto::AES aes;

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

// class to sample Gaussian noise for el gamal encryption
class GaussianSampler {
public:
  // using a singleton so the config file is only read once
  static GaussianSampler& getInstance() {
    static GaussianSampler instance("uniform.config");
    return instance;
  }

  uint32_t tail() const { return this->_tail; }

  // sample a value; if `zero` is true then use the distribution around 0
  //  otherwise use the distribution around p/2 (where p is the El Gamal prime)
  int get(bool zero) const;

private:
  GaussianSampler(std::string config_file);

  uint32_t stddev;
  uint32_t bits;
  uint32_t _tail;
  std::vector<BitString> zero_dist;
  std::vector<BitString> one_dist;
};

// uniformly sample a value less than `max`
uint32_t sampleLessThan(uint32_t max);

// uniformly sample `size` values less than `max`
std::vector<uint32_t> sampleVector(int size, uint32_t max);

// uniformly sample `size` distinct values less than `max`
std::vector<uint32_t> sampleDistinct(int size, uint32_t max);
