#include "util/random.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <boost/filesystem.hpp>
#include <openssl/bn.h>
#include <openssl/evp.h>

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
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("[PRF<uint32_t>] EVP_CIPHER_CTX_new error");
  }

  // use AES encryption in CBC mode
  if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, this->key.data(), nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("[PRF<uint32_t>] EVP_EncryptInit_ex2 error");
  }

  uint32_t output;

  // largest multiple of max within uint32_t
  uint64_t max_multiple = ((uint64_t) UINT32_MAX + 1) - (((uint64_t) UINT32_MAX + 1) % max);

  // counter added to input so we can get multiple values per `x` (for rejection sampling)
  uint32_t counter = 0;

  // sample until you get value below max_multiple (to ensure a uniform distribution)
  while (true) {
    // function input is the AES input
    BitString input = x + BitString::fromUInt(counter, 32);

    // need an extra block size for padding, I guess?
    std::vector<unsigned char> outbytes(2 * BLOCK_SIZE);

    int outl1;
    if (EVP_EncryptUpdate(ctx, outbytes.data(), &outl1, input.data(), input.nBytes()) != 1) {
      EVP_CIPHER_CTX_free(ctx);
      throw std::runtime_error("[PRF<uint32_t>] EVP_EncryptUpdate error");
    }

    int outl2;
    if (EVP_EncryptFinal_ex(ctx, outbytes.data() + outl1, &outl2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }

    // should reduce to a single BLOCK_SIZE
    outbytes.resize(outl1 + outl2);

    output = BitString(outbytes).toUInt();
    if (output < max_multiple) { break; }

    counter++;
  }

  EVP_CIPHER_CTX_free(ctx);
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
BitString PRF<BitString>::operator()(BitString x, uint32_t bits) const {
  if (x.nBytes() > BLOCK_SIZE) {
    throw std::runtime_error("[PRF<BitString>] input too large");
  }
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("[PRF<BitString>] EVP_CIPHER_CTX_new error");
  }

  // set the initialization vector as the input x
  std::vector<unsigned char> iv = x.toBytes();
  iv.resize(BLOCK_SIZE);

  // use AES encryption in CTR mode
  if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, this->key.data(), iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("[PRF<BitString>] EVP_EncryptInit_ex2 error");
  }

  // output is initially sized to the minimum multiple of the block size we need
  const size_t blocks = (((bits + 7) / 8) + BLOCK_SIZE - 1) / BLOCK_SIZE;
  std::vector<unsigned char> output(blocks * BLOCK_SIZE);

  // run aes until we have enough pseudorandom bits
  int outl;
  std::vector<unsigned char> input(16, 0);
  for (size_t i = 0; i < output.size(); i += outl) {
    if (EVP_EncryptUpdate(ctx, &output[i], &outl, input.data(), input.size()) != 1) {
      EVP_CIPHER_CTX_free(ctx);
      throw std::runtime_error("[PRF<BitString>] EVP_EncryptUpdate error");
    }
  }

  EVP_CIPHER_CTX_free(ctx);

  // truncate to the number of bytes we actually need
  output.resize((bits + 7) / 8);

  // ensure any extra bits are 0 (for internal consistancy)
  output[output.size() - 1] &= (0xFF >> ((output.size() * 8) - bits));

  return BitString(output, bits);
}

template<>
BitString PRF<BitString>::operator()(uint32_t x, uint32_t max) const {
  return this->operator()(BitString::fromUInt(x, 32), max);
}

template<>
BitString PRF<BitString>::operator()(std::pair<uint32_t, uint32_t> x, uint32_t max) const {
  return this->operator()(
    BitString::fromUInt(x.first, 32) + BitString::fromUInt(x.second, 32), max
  );
}

////////////////////////////////////////////////////////////////////////////////
// GAUSSIAN SAMPLER
////////////////////////////////////////////////////////////////////////////////

GaussianSampler::GaussianSampler(std::string filename) {
  boost::filesystem::path srcfn(__FILE__);
  std::string fullpath = (srcfn.parent_path() / filename).string();
  std::cout << "[GaussianSampler] reading config file at " << fullpath << std::endl;

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

  // probability weights for each possible observation
  uint32_t total_weight = 0;
  while (std::getline(file, line)) {
    uint32_t weight = std::stoi(line);
    total_weight += weight;
    this->cutoffs.push_back(BitString::fromUInt(total_weight, this->bits));
  }
  this->_tail = this->cutoffs.size();
}

int GaussianSampler::get() {
  // TODO: LAMBDA is more than we need here; connected to bug in BitString::sample()
  BitString randomness = BitString::sample(LAMBDA);
  BitString uniform_sample = randomness[{0, this->bits}];

  int obs = 0;
  for (; obs < this->cutoffs.size(); obs++) {
    if (uniform_sample < this->cutoffs[obs]) { break; }
  }

  return (randomness[this->bits] ? -obs : obs);
}
