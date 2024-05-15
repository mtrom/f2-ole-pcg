#include <gtest/gtest.h>

// allows us to test private methods
#define protected public
#define private public

#include "./fixtures.cxx"

class sVOLETests : public MockedCorrelations, public testing::Test { };

TEST_F(sVOLETests, Get) {
  std::vector<sVOLE> svoles = this->mocksVOLE(3, LAMBDA, LAMBDA);

  ASSERT_EQ(svoles[0].remaining(), svoles[1].remaining());
  ASSERT_EQ(svoles[0].remaining(), svoles[2].remaining());

  BitString delta = svoles[0].delta ^ svoles[1].delta ^ svoles[2].delta;
  BitString zero(LAMBDA);

  while (svoles[0].remaining() > 0) {
    auto p0 = svoles[0].get();
    auto p1 = svoles[1].get();
    auto p2 = svoles[2].get();

    bool s = p0.first ^ p1.first ^ p2.first;
    BitString actual = p0.second ^ p1.second ^ p2.second;
    ASSERT_EQ(actual, s ? delta : zero);
  }
}

TEST_F(sVOLETests, GetMany) {
  std::vector<sVOLE> svoles = this->mocksVOLE(3, LAMBDA, LAMBDA);

  ASSERT_EQ(svoles[0].remaining(), svoles[1].remaining());
  ASSERT_EQ(svoles[0].remaining(), svoles[2].remaining());

  BitString delta = svoles[0].delta ^ svoles[1].delta ^ svoles[2].delta;
  BitString zero(LAMBDA);

  auto p0 = svoles[0].get(LAMBDA / 2);
  auto p1 = svoles[1].get(LAMBDA / 2);
  auto p2 = svoles[2].get(LAMBDA / 2);

  BitString s = p0.first ^ p1.first ^ p2.first;

  ASSERT_EQ(p0.second.size(), p1.second.size());
  ASSERT_EQ(p0.second.size(), p2.second.size());

  for (size_t i = 0; i < p0.second.size(); i++) {
    BitString actual = p0.second[i] ^ p1.second[i] ^ p2.second[i];
    ASSERT_EQ(actual, s[i] ? delta : zero);
  }
}

TEST_F(sVOLETests, Reserve) {
  std::vector<sVOLE> svoles = this->mocksVOLE(3, LAMBDA, LAMBDA);

  ASSERT_EQ(svoles[0].remaining(), svoles[1].remaining());
  ASSERT_EQ(svoles[0].remaining(), svoles[2].remaining());

  BitString delta = svoles[0].delta ^ svoles[1].delta ^ svoles[2].delta;
  BitString zero(LAMBDA);

  std::vector<sVOLE> reserved({
    svoles[0].reserve(LAMBDA / 2),
    svoles[1].reserve(LAMBDA / 2),
    svoles[2].reserve(LAMBDA / 2)
  });

  while (svoles[0].remaining() > 0) {
    auto p0 = svoles[0].get();
    auto p1 = svoles[1].get();
    auto p2 = svoles[2].get();

    bool s = p0.first ^ p1.first ^ p2.first;
    BitString actual = p0.second ^ p1.second ^ p2.second;
    ASSERT_EQ(actual, s ? delta : zero);
  }

  while (reserved[0].remaining() > 0) {
    auto p0 = reserved[0].get();
    auto p1 = reserved[1].get();
    auto p2 = reserved[2].get();

    bool s = p0.first ^ p1.first ^ p2.first;
    BitString actual = p0.second ^ p1.second ^ p2.second;
    ASSERT_EQ(actual, s ? delta : zero);
  }
}
