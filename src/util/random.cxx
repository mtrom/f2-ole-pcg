#include "util/random.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <boost/filesystem.hpp>
#include <gmpxx.h>
#include <openssl/bn.h>

#include <cryptoTools/Common/block.h>
#include <cryptoTools/Crypto/AES.h>

using namespace osuCrypto;

uint32_t sampleLessThan(uint32_t max) {
  // convert max to an OpenSSL BIGNUM
  BIGNUM* bn_max = BN_new();
  BN_set_word(bn_max, max);

  // generate the output
  BIGNUM* bn_out = BN_new();
  BN_rand_range(bn_out, bn_max);

  // convert back to uint64_t
  uint32_t out = BN_get_word(bn_out);

  // free used memory
  BN_clear_free(bn_max);
  BN_clear_free(bn_out);

  return out;
}

std::vector<uint32_t> sampleVector(int size, uint32_t max) {
  std::vector<uint32_t> out(size);
  for (int i = 0; i < size; i++) {
    out[i] = sampleLessThan(max);
  }
  return out;
}

std::vector<uint32_t> sampleDistinct(int size, uint32_t max) {
  std::vector<uint32_t> out;
  while (out.size() < size) {
    uint32_t sampled = sampleLessThan(max);
    if (std::find(out.begin(), out.end(), sampled) == out.end()) {
      out.push_back(sampled);
    }
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// INTEGER PRF
////////////////////////////////////////////////////////////////////////////////

template<>
uint32_t PRF<uint32_t>::operator()(BitString x, uint32_t max) const {
  uint32_t output;

  // largest multiple of max within uint32_t
  uint64_t max_multiple = ((uint64_t) UINT32_MAX + 1) - (((uint64_t) UINT32_MAX + 1) % max);

  // counter added to input so we can get multiple values per `x` (for rejection sampling)
  uint32_t counter = 0;

  // sample until you get value below max_multiple (to ensure a uniform distribution)
  while (true) {
    BitString input = x + BitString::fromUInt(counter, 32);
    input.resize(BLOCK_SIZE * 8);
    block b = this->aes.ecbEncBlock(toBlock(input.data()));
    std::vector<unsigned char> bytes(BLOCK_SIZE);

    memcpy(bytes.data(), &b, sizeof(block));

    output = BitString(bytes).toUInt();
    if (output < max_multiple) { break; }

    counter++;
  }

  return output % max;
}

template<>
uint32_t PRF<uint32_t>::operator()(uint32_t x, uint32_t max) const {
  return this->operator()(BitString::fromUInt(x, 32), max);
}

template<>
uint32_t PRF<uint32_t>::operator()(std::pair<uint32_t, uint32_t> x, uint32_t max) const {
  return this->operator()(
    BitString::fromUInt(x.first, 32) + BitString::fromUInt(x.second, 32), max
  );
}

template class PRF<uint32_t>;

////////////////////////////////////////////////////////////////////////////////
// BITSTRING PRF
////////////////////////////////////////////////////////////////////////////////

template<>
BitString PRF<BitString>::operator()(uint32_t x, uint32_t bits) const {

  // output is sized to the minimum multiple of the block size we need
  const size_t blocks = (((bits + 7) / 8) + BLOCK_SIZE - 1) / BLOCK_SIZE;
  std::vector<block> output(blocks);

  this->aes.ecbEncCounterMode((uint64_t) x << 32, blocks, output.data());
  std::vector<unsigned char> bytes(output.size() * BLOCK_SIZE);

  unsigned char* ptr = bytes.data();
  for (auto i = 0; i < output.size(); i++) {
    memcpy(ptr, output[i].data(), sizeof(block));
  }

  return BitString(ptr, bits);
}

template<>
BitString PRF<BitString>::operator()(BitString x, uint32_t max) const {
  throw std::runtime_error("not implemented");
}

template<>
BitString PRF<BitString>::operator()(std::pair<uint32_t, uint32_t> x, uint32_t max) const {
  throw std::runtime_error("not implemented");
}

////////////////////////////////////////////////////////////////////////////////
// GAUSSIAN SAMPLER
////////////////////////////////////////////////////////////////////////////////

GaussianSampler::GaussianSampler(std::string filename) {
  boost::filesystem::path srcfn(__FILE__);
  std::string fullpath = (srcfn.parent_path() / filename).string();

  std::ifstream file(fullpath);
  if (!file.is_open()) {
    throw std::runtime_error("[GaussianSampler] failure opening configuration file");
  }

  std::string line;

  // first line is standard deviation / sigma value
  if (std::getline(file, line)) {
    this->stddev = std::stoi(line);
  } else {
    throw std::runtime_error("[GaussianSampler] failure in parsing configuration file");
  }

  // second line is the number of bits of uniform randomness needed per sample
  if (std::getline(file, line)) {
    this->bits = std::stoi(line);
  } else {
    throw std::runtime_error("[GaussianSampler] failure in parsing configuration file");
  }

  // third line is the cutoff in the distribution tail
  if (std::getline(file, line)) {
    this->_tail = std::stoi(line);
  } else {
    throw std::runtime_error("[GaussianSampler] failure in parsing configuration file");
  }

  // probability weights for each possible observation in the zero distribution
  mpz_class total = 0;
  mpz_class max_value = (mpz_class(1) << 80) - 1;  // 2^80 - 1

  for (uint32_t i = 0; i < this->_tail; i++) {
    if (!std::getline(file, line)) {
      throw std::runtime_error("[GaussianSampler] config file too short");
    }

    total += mpz_class(line);
    if (total > max_value) { total = max_value; }

    BitString cutoff(this->bits);
    mpz_export(cutoff.data(), nullptr, -1, 1, 0, 0, total.get_mpz_t());
    this->zero_dist.push_back(cutoff);
  }

  // probability weights for each possible observation in the one distribution
  total = 0;
  for (uint64_t i = 0; i < this->_tail; i++) {
    if (!std::getline(file, line)) {
      throw std::runtime_error("[GaussianSampler] config file too short");
    }
    total += mpz_class(line);
    if (total > max_value) { total = max_value; }
    BitString cutoff(this->bits);
    mpz_export(cutoff.data(), nullptr, -1, 1, 0, 0, total.get_mpz_t());
    this->one_dist.push_back(cutoff);
  }
}

int GaussianSampler::get(bool zero) const {
  // TODO: LAMBDA is more than we need here; connected to bug in BitString::sample()
  BitString randomness = BitString::sample(LAMBDA);
  BitString uniform_sample = randomness[{0, this->bits}];

  const std::vector<BitString>* dist = (zero ? &this->zero_dist : &this->one_dist);

  int obs = 0;
  for (; obs < dist->size(); obs++) {
    if (uniform_sample < (*dist)[obs]) { break; }
  }

  // accounting for the end of the tail
  if (obs == dist->size()) { obs--; }

  // different because zero is symmetrical on 0 while one is not
  if (randomness[this->bits] && zero) { return -obs; }
  else if (randomness[this->bits])    { return -obs - 1; }
  else                                { return obs; }
}
