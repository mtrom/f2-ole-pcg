#include <gtest/gtest.h>

#include "util/bitstring.hpp"

TEST(BitStringTests, AllZeroes) {
  BitString bs(10);
  ASSERT_EQ(bs.size(), 10);
  for (size_t i = 0; i < bs.size(); i++) {
    EXPECT_FALSE(bs[i]);
  }
}

TEST(BitStringTests, GetSize) {
  BitString bs = BitString::sample(77);
  EXPECT_EQ(bs.size(), 77);
  EXPECT_THROW(bs[79], std::out_of_range);
}

TEST(BitStringTests, ToString) {
  BitString bs(std::vector<unsigned char>({3, 234}));
  EXPECT_EQ(bs.toString(), "1100000001010111");
}

TEST(BitStringTests, FromString) {
  std::string str = "11010001";
  BitString bs(str);
  ASSERT_EQ(bs.size(), str.size());
  for (size_t i = 0; i < str.size(); i++) {
    EXPECT_EQ(bs[i], str[i] == '1');
  }
}

TEST(BitStringTests, AssignVariable) {
  BitString bs(std::vector<unsigned char>({3, 234}));

  BitString copy = bs;
  EXPECT_EQ(copy.size(), 16);
  for (size_t i = 0; i < bs.size(); i++) {
    EXPECT_EQ(bs[i], copy[i]);
  }
}

TEST(BitStringTests, SetBit) {
  BitString bs(std::vector<unsigned char>({3, 234}));
  for (size_t i = 0; i < 16; i++) {
    bs[i] = 1;
  }
  for (size_t i = 0; i < 16; i++) {
    ASSERT_TRUE(bs[i]);
  }
}

TEST(BitStringTests, Sample) {
  BitString bs = BitString::sample(77);
  EXPECT_EQ(bs.size(), 77);

  bool only_zeros = true;
  for (size_t i = 0; i < bs.size(); i++) {
    if (bs[i]) {
      only_zeros = false;
      break;
    }
  }
  ASSERT_FALSE(only_zeros);
}

TEST(BitStringTests, SampleSingleBit) {
  BitString bs = BitString::sample(1);
  EXPECT_EQ(bs.size(), 1);
}

TEST(BitStringTests, Compare) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42}));
  BitString c(std::vector<unsigned char>({89, 42}));

  EXPECT_NE(a, b);
  EXPECT_EQ(b, c);
  EXPECT_NE(a, c);
}

TEST(BitStringTests, LessThan) {
  BitString a("000000001010");
  BitString b("000000001010");
  BitString c("000000101010");
  BitString d("000010111010");
  BitString e("110110111010");
  BitString f("111110111010");

  EXPECT_FALSE(a < a);
  EXPECT_FALSE(a < b);
  EXPECT_LT(a, c);
  EXPECT_LT(a, d);
  EXPECT_LT(a, e);
  EXPECT_LT(a, f);
  EXPECT_FALSE(c < b);
  EXPECT_FALSE(c < c);
  EXPECT_LT(c, d);
  EXPECT_LT(c, e);
  EXPECT_LT(c, f);
  EXPECT_FALSE(d < b);
  EXPECT_FALSE(d < c);
  EXPECT_FALSE(d < d);
  EXPECT_LT(d, e);
  EXPECT_LT(d, f);
  EXPECT_FALSE(e < b);
  EXPECT_FALSE(e < c);
  EXPECT_FALSE(e < d);
  EXPECT_FALSE(e < e);
  EXPECT_LT(e, f);
  EXPECT_FALSE(f < b);
  EXPECT_FALSE(f < c);
  EXPECT_FALSE(f < d);
  EXPECT_FALSE(f < e);
  EXPECT_FALSE(f < f);
}

TEST(BitStringTests, Xor) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42}));
  BitString expected(std::vector<unsigned char>({3 ^ 89, 234 ^ 42}));

  BitString actual = a ^ b;
  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, XorMismatch) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42, 10}));
  EXPECT_THROW(BitString actual = a ^ b, std::domain_error);
}

TEST(BitStringTests, XorAssign) {
  BitString actual(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42}));
  BitString expected(std::vector<unsigned char>({3 ^ 89, 234 ^ 42}));

  actual ^= b;
  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, AND) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42}));
  BitString expected(std::vector<unsigned char>({3 & 89, 234 & 42}));

  BitString actual = a & b;
  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, AndMismatch) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42, 10}));
  EXPECT_THROW(BitString actual = a & b, std::domain_error);
}

TEST(BitStringTests, AndAssign) {
  BitString actual(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42}));
  BitString expected(std::vector<unsigned char>({3 & 89, 234 & 42}));

  actual &= b;
  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, Or) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42}));
  BitString expected(std::vector<unsigned char>({3 | 89, 234 | 42}));

  BitString actual = a | b;
  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, OrMismatch) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42, 10}));
  EXPECT_THROW(BitString actual = a | b,std::domain_error);
}

TEST(BitStringTests, OrAssign) {
  BitString actual(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({89, 42}));
  BitString expected(std::vector<unsigned char>({3 | 89, 234 | 42}));

  actual |= b;
  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, Not) {
  BitString actual(std::vector<unsigned char>({3, 234}));
  BitString expected(std::vector<unsigned char>({252, 21}));

  EXPECT_EQ(~actual, expected);
}

TEST(BitStringTests, InnerProductZeros) {
  BitString a = BitString::sample(128);
  EXPECT_FALSE(a * ~a);
}

TEST(BitStringTests, InnerProductTrue) {
  BitString a(std::vector<unsigned char>({3, 234}));
  BitString b(std::vector<unsigned char>({63, 142}));
  EXPECT_TRUE(a * b);
}

TEST(BitStringTests, InnerProductFalse) {
  BitString a(std::vector<unsigned char>({3, 235}));
  BitString b(std::vector<unsigned char>({63, 143}));
  EXPECT_FALSE(a * b);
}

TEST(BitStringTests, TensorProduct) {
  BitString a(std::vector<unsigned char>({85}));
  BitString b(std::vector<unsigned char>({63, 143}));
  BitString expected(std::vector<unsigned char>({
    63, 143, 0, 0, 63, 143, 0, 0, 63, 143, 0, 0, 63, 143, 0, 0
  }));
  BitString actual = a.tensor(b);

  ASSERT_EQ(actual.toString(), expected.toString());
}

TEST(BitStringTests, GetPrefix) {
  BitString bs(std::vector<unsigned char>({3, 234}));
  BitString expected(std::vector<unsigned char>({3}));
  BitString actual = bs[{0, 8}];

  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, GetSubstring) {
  BitString bs(std::vector<unsigned char>({0x30, 0xA4}));
  BitString expected(std::vector<unsigned char>({0x43}));
  BitString actual = bs[{4, 12}];

  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, Expand) {
  BitString bs(std::vector<unsigned char>({0xF0}));
  std::vector<unsigned char> expanded = bs.expand();

  EXPECT_EQ(expanded.size(), 8);
  for (size_t i = 0; i < 8; i++) {
    if (i < 4) { EXPECT_EQ(expanded[i], (unsigned char) 0); }
    else       { EXPECT_EQ(expanded[i], (unsigned char) 1); }
  }
}

TEST(BitStringTests, FromUInt) {
  BitString bs = BitString::fromUInt(42, 8);
  std::string actual = bs.toString();
  EXPECT_EQ(actual, "01010100");
}

TEST(BitStringTests, ToUInt32Bits) {
  BitString bs(std::vector<unsigned char>({0x2A}));
  uint32_t actual = bs.toUInt();
  EXPECT_EQ(actual, 42);
}

TEST(BitStringTests, ToUInt5Bits) {
  BitString bs(std::vector<unsigned char>({0x04}), 5);
  uint32_t actual = bs.toUInt();
  EXPECT_EQ(actual, 4);
}

TEST(BitStringTests, ToUIntsOne) {
  BitString bs(std::vector<unsigned char>({0x2A}));
  std::vector<uint32_t> actual = bs.toUInts(8);
  ASSERT_EQ(actual.size(), 1);
  EXPECT_EQ(actual[0], 42);
}

TEST(BitStringTests, ToUInts) {
  BitString bs(std::vector<unsigned char>({0x2A, 0xEA, 0xF1}));
  std::vector<uint32_t> actual = bs.toUInts(8);
  ASSERT_EQ(actual.size(), 3);
  EXPECT_EQ(actual[0], 42);
  EXPECT_EQ(actual[1], 234);
  EXPECT_EQ(actual[2], 241);
}

TEST(BitStringTests, ToUIntsThreeBits) {
  BitString bs(std::vector<unsigned char>({0x2A, 0xEA, 0xF1}));
  std::vector<uint32_t> actual = bs.toUInts(3);

  ASSERT_EQ(actual.size(), 8);
  EXPECT_EQ(actual[0], 2);
  EXPECT_EQ(actual[1], 5);
  EXPECT_EQ(actual[2], 0);
  EXPECT_EQ(actual[3], 5);
  EXPECT_EQ(actual[4], 6);
  EXPECT_EQ(actual[5], 3);
  EXPECT_EQ(actual[6], 4);
  EXPECT_EQ(actual[7], 7);
}

TEST(BitStringTests, Concat) {
  BitString a(std::vector<unsigned char>({0xC0}));
  BitString b(std::vector<unsigned char>({0x42}));

  BitString actual = a + b;
  BitString expected(std::vector<unsigned char>({0xC0, 0x42}));

  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, ConcatInPlace) {
  BitString actual(std::vector<unsigned char>({0xC0}));
  BitString b(std::vector<unsigned char>({0x42}));

  actual += b;
  BitString expected(std::vector<unsigned char>({0xC0, 0x42}));

  EXPECT_EQ(actual, expected);
}


TEST(BitStringTests, ConcatInterByte) {
  BitString a(std::vector<unsigned char>({0xFF, 0x01}), 11);
  BitString b(std::vector<unsigned char>({0xFF, 0x03}), 13);

  //   1111 1111 100
  // + 1111 1111 1100 0
  // = 1111 1111 1001 1111 1111 1000
  BitString actual = a + b;
  BitString expected(std::vector<unsigned char>({0xFF, 0xF9, 0x1F}), 24);

  EXPECT_EQ(actual, expected);
}

TEST(BitStringTests, ConcatUInts) {
  BitString bs = BitString::fromUInt(24, 13);
  bs += BitString::fromUInt(4201, 13);
  bs += BitString::fromUInt(2024, 13);

  std::vector<uint32_t> ints = bs.toUInts(13);

  EXPECT_EQ(ints.size(), 3);
  EXPECT_EQ(ints[0], 24);
  EXPECT_EQ(ints[1], 4201);
  EXPECT_EQ(ints[2], 2024);
}

TEST(BitStringTests, ConstructFromPtr) {
  std::vector<unsigned char> bytes({0xAA, 0xBB, 0xCC});
  BitString a(bytes.data(), 8);
  BitString b(bytes.data() + 1, 8);
  BitString c(bytes.data() + 2, 5);

  EXPECT_EQ(a.toString(), "01010101");
  EXPECT_EQ(b.toString(), "11011101");
  EXPECT_EQ(c.toString(), "00110");
}

TEST(BitStringTests, AESDouble) {
  BitString key = BitString::sample(128);
  size_t size = 256;

  BitString expanded = key.aes(size);

  EXPECT_EQ(expanded.size(), size);

  // hard to test if it worked, just make sure it isn't an all 0s string
  EXPECT_NE(expanded.weight(), 0);
}

TEST(BitStringTests, AESSmall) {
  BitString key = BitString::sample(128);
  size_t size = 136;

  BitString expanded = key.aes(size);

  EXPECT_EQ(expanded.size(), size);

  // hard to test if it worked, just make sure it isn't an all 0s string
  EXPECT_NE(expanded.weight(), 0);
}


TEST(BitStringTests, AESBig) {
  BitString key = BitString::sample(128);
  size_t size = 1024;

  BitString expanded = key.aes(size);

  EXPECT_EQ(expanded.size(), size);

  // hard to test if it worked, just make sure it isn't an all 0s string
  EXPECT_NE(expanded.weight(), 0);
}

TEST(BitStringTests, AESDeterministic) {
  BitString a(std::vector<unsigned char>({0xAA, 0xBB, 0xCC, 0xDD}));
  BitString b(std::vector<unsigned char>({0xAA, 0xBB, 0xCC, 0xDD}));
  size_t size = 1024;

  BitString aout = a.aes(size);
  BitString bout = b.aes(size);

  EXPECT_EQ(aout, bout);
}

TEST(BitStringTests, AESSimilarInputDifferentOutput) {
  BitString a(std::vector<unsigned char>({0xAA, 0xBB, 0xCC, 0xDD}));
  BitString b(std::vector<unsigned char>({0xAB, 0xBB, 0xCC, 0xDD}));
  size_t size = 1024;

  BitString aout = a.aes(size);
  BitString bout = b.aes(size);

  // it is possible this happens naturely, but it is very unlikely
  EXPECT_LT(16, (aout ^ bout).weight());
}

TEST(ECCTests, EncodeDecode) {
  std::vector<BitString> messages({
    BitString("00"), BitString("01"), BitString("10"), BitString("11")
  });
  std::vector<BitString> errors({
    BitString("1000"), BitString("0100"), BitString("0010"), BitString("0001")
  });

  // check every possible message / errors combo
  for (const BitString& message : messages) {
    for (const BitString& error : errors) {
      BitString encoded = ECC::encode(message);
      BitString decoded = ECC::decode(encoded ^ error);
      ASSERT_EQ(message.toString(), decoded.toString());
    }
  }
}

TEST(ECCTests, EncodeOddLengthMessage) {
  std::vector<BitString> messages({
    BitString("000"), BitString("010"), BitString("100"), BitString("110"),
    BitString("001"), BitString("011"), BitString("101"), BitString("111")
  });
  std::vector<BitString> errors({
    BitString("10001000"), BitString("01001000"), BitString("00101000"), BitString("00011000"),
    BitString("10000100"), BitString("01000100"), BitString("00100100"), BitString("00010100"),
    BitString("10000010"), BitString("01000010"), BitString("00100010"), BitString("00010010"),
    BitString("10000001"), BitString("01000001"), BitString("00100001"), BitString("00010001"),
  });

  // check every possible message / errors combo
  for (const BitString& message : messages) {
    for (const BitString& error : errors) {
      BitString encoded = ECC::encode(message);
      BitString decoded = ECC::decode(encoded ^ error);
      BitString truncated = decoded[{0, 3}];
      ASSERT_EQ(message.toString(), truncated.toString());
    }
  }
}

TEST(ECCTests, LinearityMessages) {
  std::vector<BitString> messages({
    BitString("00"), BitString("01"), BitString("10"), BitString("11")
  });
  std::vector<BitString> errors({
    BitString("1000"), BitString("0100"), BitString("0010"), BitString("0001")
  });

  // check every pair of messages that the homomorphism works
  for (const BitString& a : messages) {
    for (const BitString& b : messages) {
      for (const BitString& error : errors) {
        BitString enca = ECC::encode(a);
        BitString encb = ECC::encode(b);

        BitString decoded = ECC::decode(enca ^ encb ^ error);
        ASSERT_EQ((a ^ b).toString(), decoded.toString());
      }
    }
  }
}

TEST(ECCTests, OddLinearity) {
  std::vector<BitString> messages({
    BitString("000"), BitString("010"), BitString("100"), BitString("110"),
    BitString("001"), BitString("011"), BitString("101"), BitString("111")
  });
  std::vector<BitString> errors({
    BitString("10001000"), BitString("01001000"), BitString("00101000"), BitString("00011000"),
    BitString("10000100"), BitString("01000100"), BitString("00100100"), BitString("00010100"),
    BitString("10000010"), BitString("01000010"), BitString("00100010"), BitString("00010010"),
    BitString("10000001"), BitString("01000001"), BitString("00100001"), BitString("00010001"),
  });

  // check every pair of messages that the homomorphism works
  for (const BitString& a : messages) {
    for (const BitString& b : messages) {
      for (const BitString& error : errors) {
        BitString enca = ECC::encode(a);
        BitString encb = ECC::encode(b);

        BitString decoded = ECC::decode(enca ^ encb ^ error)[{0, 3}];
        ASSERT_EQ((a ^ b).toString(), decoded.toString());
      }
    }
  }
}

TEST(ECCTests, LongMessage) {
  BitString a = BitString::sample(128);
  BitString b = BitString::sample(128);

  BitString enca = ECC::encode(a);
  BitString encb = ECC::encode(b);

  BitString error(std::vector<unsigned char>({
    0x11, 0x12, 0x14, 0x18, 0x21, 0x22, 0x24, 0x28,
    0x41, 0x42, 0x44, 0x48, 0x81, 0x82, 0x84, 0x88,
    0x11, 0x12, 0x14, 0x18, 0x21, 0x22, 0x24, 0x28,
    0x41, 0x42, 0x44, 0x48, 0x81, 0x82, 0x84, 0x88
  }));

  BitString decoded = ECC::decode(enca ^ encb ^ error);
  ASSERT_EQ((a ^ b).toString(), decoded.toString());
}
