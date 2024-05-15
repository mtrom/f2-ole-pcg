#include <gtest/gtest.h>

#include "pkg/enc.hpp"

#include "./fixtures.cxx"

class EncTests : public MockedCorrelations, public testing::Test { };

TEST_F(EncTests, ThreeParty) {
  EncryptionParams params(BitString::sample(LAMBDA), LAMBDA, LAMBDA);
  auto triples = this->mockTriples(3, LAMBDA);

  LPN::Encryptor evaluator(params, &triples[0], true);
  LPN::Encryptor garbler00(params, &triples[1]);
  LPN::Encryptor garbler01(params, &triples[2]);

  std::vector<BitString> keys({
    BitString::sample(LAMBDA),
    BitString::sample(LAMBDA),
    BitString::sample(LAMBDA)
  });

  std::vector<BitString> shares({
    BitString::sample(LAMBDA),
    BitString::sample(LAMBDA),
    BitString::sample(LAMBDA)
  });

  BitString ciphertext = (
    evaluator.encrypt(keys[0], shares[0])
    ^ garbler00.encrypt(keys[1], shares[1])
    ^ garbler01.encrypt(keys[2], shares[2])
  );

  BitString actual = evaluator.decrypt(keys[0] ^ keys[1] ^ keys[2], ciphertext);
  BitString expected = shares[0] ^ shares[1] ^ shares[2];
  ASSERT_EQ(expected, actual);
}

TEST_F(EncTests, LambdaParties) {
  EncryptionParams params(BitString::sample(LAMBDA), LAMBDA, LAMBDA);
  auto triples = this->mockTriples(LAMBDA, LAMBDA);

  LPN::Encryptor decrypter(params, &triples[0], true);
  BitString key = BitString::sample(LAMBDA);
  BitString expected = BitString::sample(LAMBDA);
  BitString ciphertext = decrypter.encrypt(key, expected);

  for (size_t i = 1; i < LAMBDA; i++) {
    LPN::Encryptor party(params, &triples[i]);

    BitString key_i = BitString::sample(LAMBDA);
    key ^= key_i;
    BitString share = BitString::sample(LAMBDA);
    expected ^= share;

    ciphertext ^= party.encrypt(key_i, share);
  }

  BitString actual = decrypter.decrypt(key, ciphertext);
  ASSERT_EQ(expected, actual);
}
