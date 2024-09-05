#pragma once

#include <set>

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
  // just sets up initial fields
  DenseMatrix(size_t height, size_t width) : width(width), rows(height) { }

  size_t width;
  std::vector<BitString> rows;
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
  std::set<uint32_t> getNonZeroElements(size_t idx) const { return points[idx]; }

  // matrix multiplication
  DenseMatrix operator*(const DenseMatrix& other) const;

  // for debugging
  std::string toString() const;

  friend class DenseMatrix;
protected:
  // just sets up initial fields
  SparseMatrix(size_t height, size_t width) : width(width), points(height) { }

  size_t width;

  // all non-zero points
  std::vector<std::set<uint32_t>> points;
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

}
