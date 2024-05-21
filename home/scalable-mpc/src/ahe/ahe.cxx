#include <sstream>

#include "ahe/ahe.hpp"

extern "C" {
#include <relic/relic_bn.h>
}

AHE::AHE() : prf(BitString::sample(LAMBDA)) {
  // randomly sample exponent & set public key
  BitString seed = BitString::sample(LAMBDA);
  this->x.randomize();
  this->h = EC::Point::mulGenerator(this->x);

  // get floor(order) as one
  EC::Number half;
  bn_rsh(half, this->curve.getOrder(), 1);
  this->one = EC::Point::mulGenerator(half + 1);
}

AHE::Ciphertext AHE::encrypt(bool plaintext) const {
  BitString bs(1);
  bs[0] = plaintext;
  return this->encrypt(bs)[0];
}

bool AHE::decrypt(AHE::Ciphertext ciphertext) const {
  std::vector<AHE::Ciphertext> vector({ciphertext});
  return this->decrypt(vector)[0];
}

std::vector<AHE::Ciphertext> AHE::encrypt(BitString plaintext) const {
  std::vector<AHE::Ciphertext> out;
  for (size_t i = 0; i < plaintext.size(); i++) {
    BitString seed = this->prf(i, EC::Point::fromHashLength * 8);
    EC::Point c1 = EC::Point::fromHash(seed.data());
    EC::Point c2 = plaintext[i] ? (c1 * this->x) + this->one : (c1 * this->x);
    out.push_back(std::make_pair(c1, c2));
  }
  return out;
}

BitString AHE::decrypt(std::vector<AHE::Ciphertext> ciphertexts) const {
  BitString out(ciphertexts.size());
  for (size_t i = 0; i < ciphertexts.size(); i++) {
    EC::Point grx = ciphertexts[i].first * this->x;
    EC::Point gb = ciphertexts[i].second - grx;
    if (!this->isZero(gb)) { out[i] = true; }
  }
  return out;
}

AHE::Ciphertext AHE::add(AHE::Ciphertext c1, AHE::Ciphertext c2) const {
  return std::make_pair(c1.first + c2.first, c1.second + c2.second);
}

AHE::Ciphertext AHE::add(AHE::Ciphertext c, bool p) const {
  if (!p) { return c; }
  else    { return std::make_pair(c.first, c.second + this->one); }
}

AHE::Ciphertext AHE::multiply(AHE::Ciphertext c, uint64_t a) const {
  throw std::runtime_error("[AHE] not implemented");
}

bool AHE::isZero(const EC::Point& point) const {
  // TODO: should this be precomputed?
  // TODO: move 330 to a parameter
  EC::Number zero;
  bn_zero(zero);
  EC::Point check = EC::Point::mulGenerator(zero);
  EC::Point g = this->curve.getGenerator();
  for (size_t i = 0; i < 330; i++) {
    if (check == point) { return true; }
    check += g;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NETWORK METHODS
////////////////////////////////////////////////////////////////////////////////

void AHE::send(std::vector<AHE::Ciphertext> ciphertexts, Channel channel, bool compress) {

  if (compress) {
    auto key = this->prf.getKey();
    channel->write(key.data(), key.size());
  }

  size_t size = EC::Point::size * ciphertexts.size() * (compress ? 1 : 2);
  std::vector<unsigned char> message(size);

  unsigned char* iter = message.data();
  for (size_t i = 0; i < ciphertexts.size(); i++) {
    if (!compress) {
      ciphertexts[i].first.toBytes(iter);
      iter += EC::Point::size;
    }
    ciphertexts[i].second.toBytes(iter);
    iter += EC::Point::size;
  }

  channel->write(message.data(), message.size());
}

std::vector<AHE::Ciphertext> AHE::receive(size_t n, Channel channel, bool compress) {

  if (compress) {
    std::vector<unsigned char> key(LAMBDA / 8);
    channel->read(key.data(), key.size());
    this->prf.setKey(key);
  }

  size_t size = EC::Point::size * n * (compress ? 1 : 2);
  std::vector<unsigned char> message(size);
  channel->read(message.data(), message.size());

  std::vector<AHE::Ciphertext> out;
  unsigned char* iter = message.data();
  for (size_t i = 0; i < n; i++) {
    EC::Point first, second;
    if (!compress) {
      first.fromBytes(iter);
      iter += EC::Point::size;
    } else {
      BitString seed = this->prf(i, EC::Point::fromHashLength * 8);
      first = EC::Point::fromHash(seed.data());
    }
    second.fromBytes(iter);
    iter += EC::Point::size;

    out.push_back(std::make_pair(first, second));
  }

  return out;
}
