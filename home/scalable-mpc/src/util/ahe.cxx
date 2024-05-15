#include <sstream>

#include "util/ahe.hpp"

AHE::AHE(int polyModulusDegree, int plaintextModulus) { }

AHE::Ciphertext AHE::encrypt(uint64_t plaintext) const {
  return std::vector<unsigned char>();
}

uint64_t AHE::decrypt(AHE::Ciphertext ciphertext) const {
  return 0;
}

std::vector<AHE::Ciphertext> AHE::encrypt(BitString plaintext) const {
  return std::vector<AHE::Ciphertext>();
}

BitString AHE::decrypt(std::vector<AHE::Ciphertext> ciphertexts) const {
  return BitString();
}

AHE::Ciphertext AHE::add(AHE::Ciphertext c1, AHE::Ciphertext c2) const {
  return std::vector<unsigned char>();
}

AHE::Ciphertext AHE::add(AHE::Ciphertext c1, bool p) const {
  return std::vector<unsigned char>();
}

AHE::Ciphertext AHE::multiply(AHE::Ciphertext c, uint64_t a) const {
  return std::vector<unsigned char>();
}

////////////////////////////////////////////////////////////////////////////////
// NETWORK METHODS
////////////////////////////////////////////////////////////////////////////////

void AHEUtils::send(std::vector<AHE::Ciphertext> ciphertexts, Channel channel) { }

std::vector<AHE::Ciphertext> AHEUtils::receive(size_t n, Channel channel) {
  return std::vector<AHE::Ciphertext>();
}
