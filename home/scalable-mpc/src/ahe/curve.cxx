#include "ahe/curve.hpp"

#include <string>
#include <stdexcept>

extern "C" {
#include "relic/relic_core.h"
#include "relic/relic_fp.h"
#include "relic/relic_util.h"
}

#if !defined(GSL_UNLIKELY)
#define GSL_UNLIKELY(x) x
#endif

#ifndef RLC_EQ
#define RLC_EQ CMP_EQ
#endif
#ifndef RLC_LT
#define RLC_LT CMP_LT
#endif
#ifndef RLC_GT
#define RLC_GT CMP_GT
#endif

#if !defined(MULTI) || ((MULTI != PTHREAD) && (MULTI != OPENMP) && (MULTI != MSVCTLS))
static_assert(0, "Relic must be built with -DMULTI=PTHREAD or -DMULTI=OPENMP");
#endif

namespace EC {

////////////////////////////////////////////////////////////////////////////////
// NUMBER
////////////////////////////////////////////////////////////////////////////////

Number::Number(const Number& num) {
  init();
  *this = num;
}

Number::Number() {
  init();
}


Number::~Number() {
  bn_clean(*this);
}

Number& Number::operator=(const Number& c) {
  *this = c.mVal;
  return *this;
}

Number& Number::operator=(const bn_t c)
{
  if(!core_get()) {
    throw std::runtime_error(
      "Relic core not initialized on this thread"
    );
  }

  bn_copy(*this, c);

  if (GSL_UNLIKELY(err_get_code())) {
    throw std::runtime_error("Relic copy error");
  }

  return *this;
}

void Number::init() {
  bn_new(mVal);
}

void Number::reduce() {
  if (!core_get()) {
    throw std::runtime_error(
      "relic core not initialized on this thread"
    );
  }

  bn_mod_basic(*this, *this, modulus());

  if (GSL_UNLIKELY(err_get_code())) {
    throw std::runtime_error("Relic mod error");
  }
}

const bn_st* Number::modulus() const { return &core_get()->ep_r; }

std::ostream& operator<<(std::ostream& out, const Number& val) {
  if (!core_get())
    throw std::runtime_error("Relic core not initialized on this thread. Construct a RCurve to initialize it.");

  auto radix = 16;
  auto size = bn_size_str(val, radix);
  std::string str(size, 0);
  bn_write_str(&str[0], size, val, radix);

  while (str.size() && str.back() == 0)
    str.resize(str.size() - 1);

  if (str.size() == 0)
    str = "0";

  out << str;
  return out;
}

std::ostream& operator<<(std::ostream& out, const Point& val) {
  if (!core_get())
    throw std::runtime_error("Relic core not initialized on this thread. Construct a RCurve to initialize it.");

  auto radix = 16;

  auto print = [radix](std::ostream& out, const fp_t& c) {

    std::string buff(RLC_FP_BYTES * 2 + 1, ' ');

    if (int64_t(buff.size()) < int64_t(fp_size_str(c, radix)))
    {
      std::cout << "buff.size() " << buff.size() << std::endl;
      std::cout << "fp_size_str " << fp_size_str(c, radix) << std::endl;
      throw std::runtime_error("");
    }
    fp_write_str(&buff[0], static_cast<int>(buff.size()), c, radix);
    if (GSL_UNLIKELY(err_get_code()))
      throw std::runtime_error("Relic write error");

    out << buff;
  };

  Point val2;

  ep_norm(val2, val);

  out << "(";
  print(out, val2.mVal->x);
  out << ", ";
  print(out, val2.mVal->y);
  out << ", ";
  print(out, val2.mVal->z);
  out << ")";

  return out;
}

////////////////////////////////////////////////////////////////////////////////
// CURVE
////////////////////////////////////////////////////////////////////////////////

Curve::Curve(uint64_t curveID) {
  if (core_get() == nullptr)
  {
    core_init();
    if (GSL_UNLIKELY(err_get_code()))
      throw std::runtime_error("Relic core init error");

    if (!curveID)
    {
      ep_param_set_any();
      if (GSL_UNLIKELY(err_get_code()))
        throw std::runtime_error("Relic set any error");
    }
  }

  if (curveID)
  {
    if (curveID != ep_param_get())
    {
      ep_param_set(curveID);
      if (GSL_UNLIKELY(err_get_code()))
        throw std::runtime_error("Relic set any error");
    }
  }
}

Point Curve::getGenerator() const {
  if (!core_get())
    throw std::runtime_error("Relic core not initialized on this thread. Construct a RCurve to initialize it.");

  Point g;
  ep_curve_get_gen(g);

  if (GSL_UNLIKELY(err_get_code()))
    throw std::runtime_error("Relic get gen error");

  return g;
}

Number Curve::getOrder() const {
  if (!core_get())
    throw std::runtime_error("Relic core not initialized on this thread. Construct a RCurve to initialize it.");

  Number g;
  ep_curve_get_ord(g);

  if (GSL_UNLIKELY(err_get_code()))
    throw std::runtime_error("Relic get order error");
  return g;
}

////////////////////////////////////////////////////////////////////////////////
// POINT
////////////////////////////////////////////////////////////////////////////////

bool Point::iszero() const {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }

  return ep_is_infty(*this);
}

Point& Point::operator=(const Point& copy) {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }

  ep_copy(*this, copy);
  return *this;
}

Point Point::operator*(const Number& multIn) const {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }

  Point r;
  ep_mul(r, *this, multIn);
  if (GSL_UNLIKELY(err_get_code()))
    throw std::runtime_error("Relic ep_mul error");
  return r;
}

Point Point::mulGenerator(const Number& n) {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }

  Point r;
  ep_mul_gen(r, n);
  if (GSL_UNLIKELY(err_get_code())) {
    throw std::runtime_error("Relic ep_mul_gen error");
  }
  return r;
}

bool Point::operator==(const Point& cmp) const {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }
  return ep_cmp(*this, cmp) == RLC_EQ;
}

bool Point::operator!=(const Point& cmp) const {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }
  return ep_cmp(*this, cmp) != RLC_EQ;
}

void Point::fromHash(const unsigned char* data, size_t len) {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }
  ep_map(*this, data, len);
}

void Point::toBytes(unsigned char* dest) const {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }
  ep_write_bin(dest, static_cast<int>(sizeBytes()), *this, 1);
  if (GSL_UNLIKELY(err_get_code())) {
    throw std::runtime_error("Relic ep_write error");
  }
}

void Point::fromBytes(unsigned char* src) {
  if (!core_get()) {
    throw std::runtime_error(
        "Relic core not initialized on this thread"
    );
  }

  ep_read_bin(*this, src, static_cast<int>(sizeBytes()));
  if (GSL_UNLIKELY(err_get_code())) {
    throw std::runtime_error("Relic ep_read error");
  }
}

}
