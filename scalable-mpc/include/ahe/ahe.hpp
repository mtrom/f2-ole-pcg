#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "ahe/curve.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/random.hpp"

class AHE {
public:
  using Ciphertext = std::pair<EC::Point, EC::Point>;

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
  Ciphertext multiply(Ciphertext c, uint64_t a) const;

  // sending over the network
  void send(std::vector<Ciphertext> ciphertexts, Channel channel, bool compress = false);
  std::vector<Ciphertext> receive(size_t n, Channel channel, bool compress = false);

  // check if a point is zero
  bool isZero(const EC::Point& point) const;
private:
  EC::Curve curve;

  // maximum number of homomorphic operations supported
  size_t max_ops;

  // El Gamal public & private keys (h = g^x)
  EC::Number x;
  EC::Point h;

  // g^[q/2] where q is the group order
  EC::Point one;

  // for hashing to curve
  PRF<BitString> prf;

  // for sampling noise
  GaussianSampler sampler;
};
