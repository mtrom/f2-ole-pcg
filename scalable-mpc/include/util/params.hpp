#pragma once

#include <cmath>
#include <stdexcept>
#include <iomanip> // For std::fixed and std::setprecision

#include "util/bitstring.hpp"

namespace LPN {

class PrimalParams {
public:
  PrimalParams(size_t n, size_t k, size_t t, size_t l) : n(n), k(k), t(t), l(l) {
    if (n <= k) {
      throw std::invalid_argument("[LPN::PrimalParams] secret vector size larger than samples");
    } else if (n <= t) {
      throw std::invalid_argument("[LPN::PrimalParams] more errors than samples");
    } else if (n % t != 0) {
      throw std::invalid_argument("[LPN::PrimalParams] errors do not fit neatly into samples");
    }
  }

  // number of lpn samples
  size_t n;

  // size of the secret vector
  size_t k;

  // number of regular error blocks
  size_t t;

  // number of non-zero entries per row in public matrix
  size_t l;

  // number of bits per error block
  size_t blockSize() const { return n / t; }

  // number of bit to represent an error position
  size_t errorBits() const { return (size_t) ceil(log2(blockSize())); }

  std::string toString() const {
    std::ostringstream out;
    out << "n = " << n << ", k = " << k;
    out << ", t = " << t << ", l = " << l;
    return out.str();
  }
};

class DualParams {
public:
  DualParams(size_t n, float c, size_t t) : n(n), c(c), t(t) {
    if (n <= t) {
      throw std::invalid_argument("[LPN::DualParams] more errors than samples");
    }
  }

  // number of lpn samples
  size_t n;

  // code expansion factor
  float c;

  // number of errors
  size_t t;

  // dual-code matrix dimension
  size_t N() const { return (size_t) ceil(n * c); }

  // number of bits per error block
  size_t blockSize() const { return (size_t) ceil((float) N() / t); }

  std::string toString() const {
    std::ostringstream out;
    out << "N = " << N() << ", t = " << t;
    out << ", c = " << std::fixed << std::setprecision(1) << c;
    return out.str();
  }
};

}

class PCGParams {
public:
  PCGParams(
    size_t size,
    BitString pkey, size_t n, size_t k, size_t tp, size_t l,
    BitString dkey, float c, size_t td
  ) : size(size), primal(n, k, tp, l), pkey(pkey), dual(k, c, td), dkey(dkey) { }

  PCGParams(
    BitString pkey, size_t n, size_t k, size_t tp, size_t l,
    BitString dkey, float c, size_t td
  ) : PCGParams(n, pkey, n, k, tp, l, dkey, c, td) { }

  // number of correlations to output
  size_t size;

  // number of parties in the protocol
  size_t parties;

  // parameters & public seed for primal lpn instance
  LPN::PrimalParams primal;
  BitString pkey;

  // parameters & public seed for dual lpn instance
  LPN::DualParams dual;
  BitString dkey;

  // parameter for equality testing
  // TODO: what should this value be?
  size_t eqTestThreshold = 3;

  size_t blocks() {
    return (size_t) ceil((float) size / primal.blockSize());
  }

  std::string toString() const {
    return (
      "[LPN::Primal] " + primal.toString() + "\n" +
      "[LPN::Dual]   " + dual.toString()
    );
  }

  size_t numRandomOTs() const {
    return 0;
  }
};

class EncryptionParams {
public:
  EncryptionParams(BitString pkey, size_t key_size, size_t msg_size)
    : key_size(key_size), pkey(pkey), msg_size(msg_size) { };

  // size of the key vector used to encrypt messages
  const size_t key_size;

  // size of the messages being encrypted
  const size_t msg_size;

  // public PRF key used to generate the public matrices
  const BitString pkey;
};
