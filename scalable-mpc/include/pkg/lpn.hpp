#pragma once

#include <memory>

#include "util/bitstring.hpp"
#include "util/params.hpp"

namespace LPN {

// abstract class that encapsulates basic binary matrix operations
class Matrix {
public:
  // element access
  virtual bool operator[](std::pair<size_t, size_t> idx) const = 0;

  // retrieve an entire row as a bitstring
  virtual BitString operator[](size_t idx) const = 0;

  // get the matrix dimensions
  virtual std::pair<size_t, size_t> dim() const = 0;

  // matrix vector multiplication
  virtual BitString operator*(const BitString& other) const = 0;

};

class MatrixProduct;

class DenseMatrix : public Matrix {
public:
  DenseMatrix() : width(0), rows(0) { }

  // sample a random matrix using `key`
  DenseMatrix(size_t height, size_t width, const BitString& key);

  // basic operations
  bool operator[](std::pair<size_t, size_t> idx) const override;
  BitString operator[](size_t idx) const override;
  std::pair<size_t, size_t> dim() const override;
  BitString operator*(const BitString& other) const override;

  // for debugging
  std::string toString() const;

  friend class SparseMatrix;
protected:
  DenseMatrix(size_t height, size_t width)
    : width(width), rows(std::make_shared<std::vector<BitString>>(height)) { }

  // using shared pointer to prevent duplication in memory
  std::shared_ptr<std::vector<BitString>> rows;
  size_t width;
};

// matrix with a constant number of non-zero elements per row
class SparseMatrix : public Matrix {
public:
  // basic operations
  bool operator[](std::pair<size_t, size_t> idx) const override;
  BitString operator[](size_t idx) const override;
  std::pair<size_t, size_t> dim() const override;
  BitString operator*(const BitString& other) const override;

  // directly get non-zero points
  std::vector<uint32_t> getNonZeroElements(size_t idx) const { return (*points)[idx]; }

  // matrix multiplication
  MatrixProduct operator*(const DenseMatrix& other) const;

  // for debugging
  std::string toString() const;

  friend class DenseMatrix;
protected:
  // just sets up initial fields
  SparseMatrix(size_t height, size_t width)
    : width(width), points(std::make_shared<std::vector<std::vector<uint32_t>>>(height)) { }

  // all non-zero points (using shared pointer to prevent duplication in memory)
  std::shared_ptr<std::vector<std::vector<uint32_t>>> points;
  size_t width;
};

class PrimalMatrix : public SparseMatrix {
public:
  PrimalMatrix() : SparseMatrix(0, 0), key(0) { };
  PrimalMatrix(const BitString& key, const PrimalParams& params);

  // just samples a key and returns a matrix; mostly for testing
  static PrimalMatrix sample(const PrimalParams& params);
private:
  BitString key;
};

class DualMatrix : public DenseMatrix {
public:
  DualMatrix() : DenseMatrix(), key(0) { };
  DualMatrix(const BitString& key, const DualParams& params);

  // just samples a key and returns a matrix; mostly for testing
  static DualMatrix sample(const DualParams& params);
private:
  BitString key;
};

class MatrixProduct {
public:
  MatrixProduct(SparseMatrix sparse, DenseMatrix dense) : sparse(sparse), dense(dense) {
    if (sparse.dim().second != dense.dim().first) {
      throw std::domain_error("[MatrixProduct] matrix dimensions mismatched");
    }
  }

  std::pair<size_t, size_t> dim() const {
    return std::make_pair(sparse.dim().first, dense.dim().second);
  }

  BitString operator[](size_t idx) const;
private:
  SparseMatrix sparse;
  DenseMatrix dense;
};

}
