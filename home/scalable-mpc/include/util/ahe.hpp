#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "util/bitstring.hpp"
#include "util/defines.hpp"

#define POLY_MODULUS_DEGREE 1024
const int PLAINTEXT_MODULUS = 2;

class AHE {
public:
  using Ciphertext = std::vector<unsigned char>;

  AHE(int polyModulusDegree = POLY_MODULUS_DEGREE, int plaintextModulus = PLAINTEXT_MODULUS);

  // generic encrypt / decrypt
  Ciphertext encrypt(uint64_t plaintext) const;
  uint64_t decrypt(Ciphertext ciphertext) const;

  // encrypt / decrypt binary strings
  std::vector<Ciphertext> encrypt(BitString plaintext) const;
  BitString decrypt(std::vector<Ciphertext> ciphertexts) const;

  // homomorphic operations
  Ciphertext add(Ciphertext c1, Ciphertext c2) const;
  Ciphertext add(Ciphertext c1, bool p) const;
  Ciphertext multiply(Ciphertext c, uint64_t a) const;
};

// TODO: move this into AHE class
namespace AHEUtils {
void send(std::vector<AHE::Ciphertext> ciphertexts, Channel channel);
std::vector<AHE::Ciphertext> receive(size_t n, Channel channel);
}
