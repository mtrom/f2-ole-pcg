#include <sstream>

#include "ahe/ahe.hpp"
#include "util/concurrency.hpp"

extern "C" {
#include <relic/relic_bn.h>
}

AHE::AHE(size_t max_ops)
  : max_ops(max_ops), prf(BitString::sample(LAMBDA)), sampler(GaussianSampler::getInstance())
{
  // randomly sample exponent & set public key
  this->x.randomize();
  this->h = EC::Point::mulGenerator(this->x);

  // get floor(order) as one
  EC::Number half;
  bn_rsh(half, this->curve.getOrder(), 1);
  this->one = EC::Point::mulGenerator(half + 1);

  // construct lookup table for decryption
  EC::Number zero;
  bn_zero(zero);
  EC::Point positives = EC::Point::mulGenerator(zero);
  EC::Point negatives = EC::Point::mulGenerator(zero);
  EC::Point g = this->curve.getGenerator();
  lookup.push_back(positives);
  for (size_t i = 0; i <= sampler.tail() * (max_ops + 1); i++) {
    positives += g;
    negatives = negatives - g;
    lookup.push_back(positives);
    lookup.push_back(negatives);
  }
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
  EC::Number zero;
  bn_zero(zero);

  return TASK_REDUCE<std::vector<AHE::Ciphertext>>(
    [this, &plaintext, &zero](size_t start, size_t end) {
      EC::Curve curve; // initialize relic on the thread
      std::vector<AHE::Ciphertext> out;
      for (size_t i = start; i < end; i++) {
        BitString seed = this->prf(i, EC::Point::fromHashLength * 8);
        EC::Point c1 = EC::Point::fromHash(seed.data());
        EC::Point c2 = plaintext[i] ? (c1 * this->x) + this->one : (c1 * this->x);

        // Gaussian noise around
        int sampled = sampler.get(!plaintext[i]);
        EC::Point noise = EC::Point::mulGenerator(zero + abs(sampled));
        c2 = (sampled >= 0) ? c2 + noise : c2 - noise;

        out.push_back(std::make_pair(c1, c2));
      }
      return out;
    }, [](std::vector<std::vector<AHE::Ciphertext>> ciphertexts) {
      std::vector<AHE::Ciphertext> out = ciphertexts[0];
      for (size_t i = 1; i < ciphertexts.size(); i++) {
        out.insert(out.end(), ciphertexts[i].begin(), ciphertexts[i].end());
      }
      return out;
  }, plaintext.size());
}

BitString AHE::decrypt(std::vector<AHE::Ciphertext> ciphertexts) const {
  return TASK_REDUCE<BitString>([this, &ciphertexts](size_t start, size_t end) {
    EC::Curve curve; // initialize relic on the thread
    BitString out(end - start);
    for (size_t i = start; i < end; i++) {
      EC::Point grx = ciphertexts[i].first * this->x;
      EC::Point gb = ciphertexts[i].second - grx;
      if (!this->isZero(gb)) { out[i - start] = true; }
    }
    return out;
  }, BitString::concat, ciphertexts.size());
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
  return std::find(lookup.begin(), lookup.end(), point) != lookup.end();
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

  std::vector<unsigned char> key(LAMBDA / 8);
  if (compress) { channel->read(key.data(), key.size()); }
  PRF<BitString> their_prf(key);

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
      BitString seed = their_prf(i, EC::Point::fromHashLength * 8);
      first = EC::Point::fromHash(seed.data());
    }
    second.fromBytes(iter);
    iter += EC::Point::size;

    out.push_back(std::make_pair(first, second));
  }

  return out;
}
