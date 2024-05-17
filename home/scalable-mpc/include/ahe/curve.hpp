#pragma once

#include <cstring>
#include <string>
#include <ostream>
#include <iostream>

extern "C" {
    #include <relic/relic_bn.h>
    #include <relic/relic_ep.h>
}

namespace EC {

class Curve;
class Point;
class EccBrick;

class Number
{
public:

  Number();
  Number(const Number& num);
  Number(Number&& moveFrom)
  {
    std::memcpy(&mVal, &moveFrom.mVal, sizeof(bn_t));
    bn_null(moveFrom.mVal);
  }

  ~Number();

  Number& operator=(const Number& c);
  Number& operator=(const bn_t c);

  operator bn_t& () { return mVal; }
  operator const bn_t& () const { return mVal; }

private:
  void init();
  void reduce();

  const bn_st* modulus() const;

public:
  bn_t  mVal;

  friend class Curve;
  friend Point;
  friend std::ostream& operator<<(std::ostream& out, const Number& val);
};
std::ostream& operator<<(std::ostream& out, const Number& val);


class Point
{
public:

  Point() { ep_new(mVal); };
  Point(const Point& copy) { ep_new(mVal); ep_copy(*this, copy); }

  Point(Point&& moveFrom)
  {
    std::memcpy(&mVal, &moveFrom.mVal, sizeof(ep_t));
    ep_null(moveFrom.mVal);
  }

  ~Point() { ep_free(mVal); }

  Point& operator=(const Point& copy);
  Point& operator=(Point&& moveFrom)
  {
    std::swap(mVal, moveFrom.mVal);
    return *this;
  }

  Point operator*(const Number& multIn) const;

  // Multiply a scalar by the generator of the elliptic curve. Unsure if this is the whole
  // curve or a prime order subgroup, but it should be the same as
  // Curve::getGenerator() * n.
  static Point mulGenerator(const Number& n);

  bool operator==(const Point& cmp) const;
  bool operator!=(const Point& cmp) const;

  // Generate randomly from a 256 bit hash. d must point to fromHashLength uniformly random
  // bytes.
  static Point fromHash(const unsigned char* d)
  {
    Point p;
    p.fromHash(d, fromHashLength);
    return p;
  }

  // Feed data[0..len] into a hash function, then map the hash to the curve.
  void fromHash(const unsigned char* data, size_t len);

  static const size_t fromHashLength = 0x20;

  uint64_t sizeBytes() const { return size; }
  void toBytes(unsigned char* dest) const;
  void fromBytes(unsigned char* src);

  bool iszero() const;

  operator ep_t& () { return mVal; }
  operator const ep_t& () const { return mVal; }

  static const uint64_t size = 1 + RLC_FP_BYTES;

  ep_t mVal;
private:
  friend Number;
  friend std::ostream& operator<<(std::ostream& out, const Point& val);
};

std::ostream& operator<<(std::ostream& out, const Point& val);

class Curve
{
public:
  Curve(uint64_t curveID = 0);

  Point getGenerator() const;
  Number getOrder() const;
private:
  friend Point;
  friend Number;
};
}
