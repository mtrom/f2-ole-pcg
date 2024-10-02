#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <cryptoTools/Crypto/RCurve.h>

#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/random.hpp"

using namespace osuCrypto;

class AHE {
public:
  using Ciphertext = std::pair<REccPoint, REccPoint>;

  // sample a random public & private key
  AHE(size_t max_ops = 1);

  // generic encrypt / decrypt
  Ciphertext encrypt(bool plaintext) const;
  bool decrypt(Ciphertext ciphertext) const;

  // encrypt / decrypt binary strings
  std::vector<Ciphertext> encrypt(BitString plaintext) const;
  BitString decrypt(std::vector<Ciphertext> ciphertexts) const;

  // homomorphic operations
  Ciphertext add(Ciphertext c1, Ciphertext c2) const;
  Ciphertext add(Ciphertext c1, bool p) const;

  // sending over the network
  void send(std::vector<Ciphertext> ciphertexts, Channel channel, bool compress = false);
  std::vector<Ciphertext> receive(size_t n, Channel channel, bool compress = false);

  // check if a point is zero
  bool isZero(const REccPoint& point) const;
private:
  REllipticCurve curve;

  // maximum number of homomorphic operations supported
  size_t max_ops;

  // El Gamal public & private keys (h = g^x)
  REccNumber x;
  REccPoint h;

  // g^[q/2] where q is the group order
  REccPoint one;

  // lookup table for decryption
  std::vector<REccPoint> lookup;

  // for hashing to curve
  PRF<BitString> prf;

  // for sampling noise
  GaussianSampler sampler;
};
