#include <sstream>

#include "ahe/ahe.hpp"

AHE::AHE(int polyModulusDegree, int plaintextModulus) { }

AHE::Ciphertext AHE::encrypt(uint64_t plaintext) const {
  throw std::runtime_error("[AHE] not implemented");
}

uint64_t AHE::decrypt(AHE::Ciphertext ciphertext) const {
  throw std::runtime_error("[AHE] not implemented");
}

std::vector<AHE::Ciphertext> AHE::encrypt(BitString plaintext) const {
  throw std::runtime_error("[AHE] not implemented");
}

BitString AHE::decrypt(std::vector<AHE::Ciphertext> ciphertexts) const {
  throw std::runtime_error("[AHE] not implemented");
}

AHE::Ciphertext AHE::add(AHE::Ciphertext c1, AHE::Ciphertext c2) const {
  throw std::runtime_error("[AHE] not implemented");
}

AHE::Ciphertext AHE::add(AHE::Ciphertext c1, bool p) const {
  throw std::runtime_error("[AHE] not implemented");
}

AHE::Ciphertext AHE::multiply(AHE::Ciphertext c, uint64_t a) const {
  throw std::runtime_error("[AHE] not implemented");
}

////////////////////////////////////////////////////////////////////////////////
// NETWORK METHODS
////////////////////////////////////////////////////////////////////////////////

void AHEUtils::send(std::vector<AHE::Ciphertext> ciphertexts, Channel channel) {
  throw std::runtime_error("[AHE] not implemented");
}

std::vector<AHE::Ciphertext> AHEUtils::receive(size_t n, Channel channel) {
  throw std::runtime_error("[AHE] not implemented");
}
