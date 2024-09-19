#pragma once

#include <set>
#include <memory>

#include "util/bitstring.hpp"
#include "util/params.hpp"
#include "util/random.hpp"

namespace LPN {

class MatrixProduct;
class PrimalMatrix;
class DualMatrix;

class EncryptionMatrix {
public:
  EncryptionMatrix() : width(0), rows(0) { }

  // sample a random matrix using `key`
  EncryptionMatrix(size_t height, size_t width, const BitString& key);

  // basic operations
  bool operator[](std::pair<size_t, size_t> idx) const;
  BitString operator[](size_t idx) const;
  std::pair<size_t, size_t> dim() const;
  BitString operator*(const BitString& other) const;

  // for debugging
  std::string toString() const;
protected:
  EncryptionMatrix(size_t height, size_t width)
    : width(width), rows(std::make_shared<std::vector<BitString>>(height)) { }

  // using shared pointer to prevent duplication in memory
  std::shared_ptr<std::vector<BitString>> rows;
  size_t width;
};

// matrix with a constant number of non-zero elements per row
class PrimalMatrix {
public:
  PrimalMatrix() { }
  PrimalMatrix(const BitString& key, const PrimalParams& params)
    : prf(std::make_shared<PRF<uint32_t>>(key)), height(params.n), width(params.k),
      sparsity(params.l) { }

  // just samples a key and returns a matrix; mostly for testing
  static PrimalMatrix sample(const PrimalParams& params) {
    return PrimalMatrix(BitString::sample(LAMBDA), params);
  }

  // matrix dimensions
  std::pair<size_t, size_t> dim() const;

  // directly get non-zero points
  std::set<uint32_t> getNonZeroElements(size_t idx) const;

  // matrix vector multiplication
  BitString operator*(const BitString& other) const;

  // matrix multiplication
  MatrixProduct operator*(const DualMatrix& other) const;

  // for debugging
  BitString operator[](size_t idx) const;
  std::string toString() const;
protected:
  std::shared_ptr<PRF<uint32_t>> prf;
  size_t height, width, sparsity;
};

class DualMatrix {
public:
  DualMatrix() { }
  DualMatrix(const BitString& key, const DualParams& params)
    : prf(std::make_shared<PRF<BitString>>(key)), height(params.n), width(params.N()) { }

  // matrix dimensions
  std::pair<size_t, size_t> dim() const { return std::make_pair(height, width); }

  // direct element access
  bool operator[](std::pair<size_t, size_t> idx) const;

  // matrix vector multiplication
  BitString operator*(const BitString& other) const;

  // just samples a key and returns a matrix; mostly for testing
  static DualMatrix sample(const DualParams& params) {
    return DualMatrix(BitString::sample(LAMBDA), params);
  }
private:
  std::shared_ptr<PRF<BitString>> prf;
  size_t height, width;
};

class MatrixProduct {
public:
  MatrixProduct() { }
  MatrixProduct(PrimalMatrix primal, DualMatrix dual) : primal(primal), dual(dual) {
    if (primal.dim().second != dual.dim().first) {
      throw std::domain_error("[MatrixProduct] matrix dimensions mismatched");
    }
  }

  std::pair<size_t, size_t> dim() const {
    return std::make_pair(primal.dim().first, dual.dim().second);
  }

  BitString operator[](size_t idx) const;
private:
  PrimalMatrix primal;
  DualMatrix dual;
};

}
