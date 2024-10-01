#include <gtest/gtest.h>

#include "ahe/ahe.hpp"
#include "test/fixtures.cxx"

#include "util/bitstring.hpp"

class AHETests : public NetworkTest { };

TEST_F(AHETests, EncryptDecryptBit) {
  AHE encrypter;
  std::vector<bool> plaintexts({false, true});
  for (bool plaintext : plaintexts) {
    AHE::Ciphertext ciphertext = encrypter.encrypt(plaintext);
    bool recovered = encrypter.decrypt(ciphertext);
    EXPECT_EQ(plaintext, recovered);
  }
}

TEST_F(AHETests, EncryptDecryptBitString) {
  AHE encrypter;
  for (uint64_t plaintext = 0; plaintext < (1 << 5); plaintext++) {
    BitString expected = BitString::fromUInt(plaintext, 8);
    std::vector<AHE::Ciphertext> ciphertexts = encrypter.encrypt(expected);
    BitString actual = encrypter.decrypt(ciphertexts);
    EXPECT_EQ(expected, actual);
  }
}

TEST_F(AHETests, EncryptAllZeros) {
  AHE encrypter;
  BitString expected(128);
  std::vector<AHE::Ciphertext> ciphertexts = encrypter.encrypt(expected);
  BitString actual = encrypter.decrypt(ciphertexts);
  EXPECT_EQ(expected, actual);
}

TEST_F(AHETests, CiphertextAddition) {
  AHE encrypter;
  for (uint64_t p0 = 0; p0 <= 1; p0++) {
    for (uint64_t p1 = 0; p1 <= 1; p1++) {
      AHE::Ciphertext c0 = encrypter.encrypt(p0);
      AHE::Ciphertext c1 = encrypter.encrypt(p1);
      AHE::Ciphertext sum = encrypter.add(c0, c1);
      uint64_t expected = (p0 + p1) % 2;
      uint64_t actual = encrypter.decrypt(sum);
      EXPECT_EQ(expected, actual);
    }
  }
}

TEST_F(AHETests, PlaintextAddition) {
  AHE encrypter;
  for (uint64_t p0 = 0; p0 <= 1; p0++) {
    for (uint64_t p1 = 0; p1 <= 1; p1++) {
      AHE::Ciphertext c0 = encrypter.encrypt(p0);
      AHE::Ciphertext sum = encrypter.add(c0, p1);
      uint64_t expected = (p0 + p1) % 2;
      uint64_t actual = encrypter.decrypt(sum);
      EXPECT_EQ(expected, actual);
    }
  }
}

TEST_F(AHETests, MultipleHomomorphicOperations) {
  size_t OPERATIONS = 128;
  AHE encrypter(OPERATIONS);

  BitString bits = BitString::sample(OPERATIONS + 1);
  std::vector<AHE::Ciphertext> ctxs = encrypter.encrypt(bits);

  AHE::Ciphertext sum = ctxs[0];
  for (size_t i = 1; i < ctxs.size(); i++) {
    sum = encrypter.add(sum, ctxs[i]);
  }

  uint64_t expected = bits.weight() % 2;
  uint64_t actual = encrypter.decrypt(sum);

  EXPECT_EQ(expected, actual);
}

TEST_F(AHETests, SendAndReceive) {
  BitString expected("10101111");
  AHE encrypter;
  auto results = this->launch(
    [&]() -> bool {
      EC::Curve curve;
      std::vector<AHE::Ciphertext> ciphertexts = encrypter.encrypt(expected);
      encrypter.send(ciphertexts, this->sch);
      return true;
    },
    [&]() -> BitString {
      EC::Curve curve;
      std::vector<AHE::Ciphertext> ciphertexts = encrypter.receive(expected.size(), this->rch);
      return encrypter.decrypt(ciphertexts);
    }
  );
  BitString actual = results.second;
  ASSERT_EQ(expected, actual);
}

TEST_F(AHETests, SendAndReceiveCompressed) {
  BitString expected("10101111");
  auto results = this->launch(
    [&]() -> AHE {
      AHE encrypter;
      std::vector<AHE::Ciphertext> ciphertexts = encrypter.encrypt(expected);
      encrypter.send(ciphertexts, this->sch, true);
      return encrypter;
    },
    [&]() -> std::vector<AHE::Ciphertext> {
      AHE receiver;
      return receiver.receive(expected.size(), this->rch, true);
    }
  );
  EC::Curve curve;
  AHE encrypter = results.first;
  BitString actual = encrypter.decrypt(results.second);
  ASSERT_EQ(expected, actual);
}
