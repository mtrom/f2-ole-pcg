#include <sstream>

#include <cryptoTools/Common/block.h>
#include <cryptoTools/Crypto/RCurve.h>

extern "C" {
#include <relic/relic_bn.h>
}

#include "ahe/ahe.hpp"
#include "util/concurrency.hpp"


using namespace osuCrypto;

AHE::AHE(size_t max_ops)
  : max_ops(max_ops), prf(BitString::sample(LAMBDA)), sampler(GaussianSampler::getInstance())
{
  // randomly sample exponent & set public key
  uint64_t seed;
  std::memcpy(&seed, BitString::sample(64).data(), sizeof(uint64_t));
  this->x.randomize(block(seed));
  this->h = REccPoint::mulGenerator(this->x);

  // get floor(order) as one
  REccNumber half;
  bn_rsh(half, this->curve.getOrder(), 1);
  this->one = REccPoint::mulGenerator(half + 1);

  // construct lookup table for decryption
  REccNumber zero;
  bn_zero(zero);
  REccPoint positives = REccPoint::mulGenerator(zero);
  REccPoint negatives = REccPoint::mulGenerator(zero);
  REccPoint g = this->curve.getGenerator();
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
  REccNumber zero;
  bn_zero(zero);

  return TASK_REDUCE<std::vector<AHE::Ciphertext>>(
    [this, &plaintext, &zero](size_t start, size_t end) {
      REllipticCurve curve; // initialize relic on the thread
      std::vector<AHE::Ciphertext> out;
      for (size_t i = start; i < end; i++) {
        BitString seed = this->prf(i, REccPoint::fromHashLength * 8);
        REccPoint c1 = REccPoint::fromHash(seed.data());
        REccPoint c2 = plaintext[i] ? (c1 * this->x) + this->one : (c1 * this->x);

        // Gaussian noise around
        int sampled = sampler.get(!plaintext[i]);
        REccPoint noise = REccPoint::mulGenerator(zero + abs(sampled));
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
    REllipticCurve curve; // initialize relic on the thread
    BitString out(end - start);
    for (size_t i = start; i < end; i++) {
      REccPoint grx = ciphertexts[i].first * this->x;
      REccPoint gb = ciphertexts[i].second - grx;
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

bool AHE::isZero(const REccPoint& point) const {
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

  size_t size = REccPoint::size * ciphertexts.size() * (compress ? 1 : 2);
  std::vector<unsigned char> message(size);

  unsigned char* iter = message.data();
  for (size_t i = 0; i < ciphertexts.size(); i++) {
    if (!compress) {
      ciphertexts[i].first.toBytes(iter);
      iter += REccPoint::size;
    }
    ciphertexts[i].second.toBytes(iter);
    iter += REccPoint::size;
  }

  channel->write(message.data(), message.size());
}

std::vector<AHE::Ciphertext> AHE::receive(size_t n, Channel channel, bool compress) {

  std::vector<unsigned char> key(LAMBDA / 8);
  if (compress) { channel->read(key.data(), key.size()); }
  PRF<BitString> their_prf(key);

  size_t size = REccPoint::size * n * (compress ? 1 : 2);
  std::vector<unsigned char> message(size);
  channel->read(message.data(), message.size());

  return TASK_REDUCE<std::vector<AHE::Ciphertext>>(
    [&compress, &message, &their_prf](size_t start, size_t end)
  {
    REllipticCurve curve; // initalize relic on this thread
    std::vector<AHE::Ciphertext> out;
    unsigned char* iter = (
      message.data() + ((compress ? 1 : 2) * REccPoint::size * start)
    );
    for (size_t i = start; i < end; i++) {
      REccPoint first, second;
      if (!compress) {
        first.fromBytes(iter);
        iter += REccPoint::size;
      } else {
        BitString seed = their_prf(i, REccPoint::fromHashLength * 8);
        first = REccPoint::fromHash(seed.data());
      }
      second.fromBytes(iter);
      iter += REccPoint::size;

      out.push_back(std::make_pair(first, second));
    }
    return out;
  }, [](std::vector<std::vector<AHE::Ciphertext>> results) {
    std::vector<AHE::Ciphertext> out;
    for (auto& result : results) {
      out.insert(out.end(), result.begin(), result.end());
    }
    return out;
  }, n);
}
