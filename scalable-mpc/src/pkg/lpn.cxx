#include "pkg/lpn.hpp"

#include <algorithm>
#include <iostream>
#include <thread>

#include "util/concurrency.hpp"
#include "util/defines.hpp"
#include "util/random.hpp"

namespace LPN {

////////////////////////////////////////////////////////////////////////////////
// DENSE MATRIX
////////////////////////////////////////////////////////////////////////////////

DenseMatrix::DenseMatrix(size_t height, size_t width, const BitString& key)
  : width(width), rows(std::make_shared<std::vector<BitString>>(height))
{
  PRF<BitString> prf(key);
  for (size_t i = 0; i < height; i++) {
    (*this->rows)[i] = prf(i, width);
  }
}

bool DenseMatrix::operator[](std::pair<size_t, size_t> idx) const {
  if (idx.first >= (*this->rows).size() || idx.second >= this->width) {
    throw std::domain_error("[DenseMatrix::operator[](std::pair)] idx out of range");
  }
  return (*rows)[idx.first][idx.second];
}

BitString DenseMatrix::operator[](size_t idx) const {
  if (idx >= (*this->rows).size()) {
    throw std::domain_error("[DualMatrix::operator[](size_t)] idx out of range");
  }
  return (*this->rows)[idx];
}

std::pair<size_t, size_t> DenseMatrix::dim() const {
  return std::make_pair((*this->rows).size(), this->width);
}

BitString DenseMatrix::operator*(const BitString& other) const {
  if (this->dim().second != other.size()) {
    throw std::domain_error("[DenseMatrix::operator*(BitString)] vector dimension mismatched");
  }
  BitString result(this->dim().first);
  for (size_t i = 0; i < (*this->rows).size(); i++) {
    result[i] = (*this->rows)[i] * other;
  }
  return result;
}

std::string DenseMatrix::toString() const {
  std::string out;
  for (size_t i = 0; i < (*this->rows).size(); i++) {
    out += (*this->rows)[i].toString() + "\n";
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// SPARSE MATRIX
////////////////////////////////////////////////////////////////////////////////

bool SparseMatrix::operator[](std::pair<size_t, size_t> idx) const {
  if (idx.first >= (*this->points).size() || idx.second >= this->width) {
    throw std::domain_error("[SparseMatrix::operator[](std::pair)] idx out of range");
  }
  return (*this->points)[idx.first].find(idx.second) != (*this->points)[idx.first].end();
}

BitString SparseMatrix::operator[](size_t idx) const {
  if (idx >= (*this->points).size()) {
    throw std::domain_error("[SparseMatrix::operator[](size_t)] idx out of range");
  }

  BitString row(this->width);
  for (uint32_t i : (*this->points)[idx]) {
    row[i] = true;
  }
  return row;
}

std::pair<size_t, size_t> SparseMatrix::dim() const {
  return std::make_pair((*this->points).size(), this->width);
}

DenseMatrix SparseMatrix::operator*(const DenseMatrix& other) const {
  if (this->dim().second != other.dim().first) {
    throw std::domain_error("[SparseMatrix::operator*] matrix dimensions mismatched");
  }
  DenseMatrix result((*this->points).size(), other.width);

  MULTI_TASK([this, &other, &result](size_t start, size_t end) {
    for (size_t row = start; row < end; row++) {
      (*result.rows)[row] = BitString(other.width);
      for (size_t col = 0; col < other.width; col++) {
        bool innerproduct = false;
        for (uint32_t point : (*this->points)[row]) {
          if (other[{point, col}]) { innerproduct = !innerproduct; }
        }
        (*result.rows)[row][col] = innerproduct;
      }
    }
  }, (*this->points).size());

  return result;
}

BitString SparseMatrix::operator*(const BitString& other) const {
  if (this->dim().second != other.size()) {
    throw std::domain_error("[SparseMatrix::operator*(BitString)] vector dimension mismatched");
  }
  BitString result(this->dim().first);
  for (size_t i = 0; i < (*this->points).size(); i++) {
    for (uint32_t point : (*this->points)[i]) {
      if (other[point]) { result[i] ^= other[point]; }
    }
  }
  return result;
}

std::string SparseMatrix::toString() const {
  std::string out;
  for (size_t i = 0; i < (*this->points).size(); i++) {
    out += this->operator[](i).toString() + "\n";
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// PRIMAL MATRIX
////////////////////////////////////////////////////////////////////////////////

PrimalMatrix::PrimalMatrix(const BitString& key, const PrimalParams& params)
  : SparseMatrix(params.n, params.k), key(key)
{
  PRF<uint32_t> prf(key);

  MULTI_TASK([this, &prf, &params](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      for (size_t j = 0; (*this->points)[i].size() < params.l; j++) {
        (*this->points)[i].insert(prf(std::make_pair(i, j), params.k));
      }
    }
  }, params.n);
}

PrimalMatrix PrimalMatrix::sample(const PrimalParams& params) {
  return PrimalMatrix(BitString::sample(LAMBDA), params);
}

////////////////////////////////////////////////////////////////////////////////
// DUAL MATRIX
////////////////////////////////////////////////////////////////////////////////

DualMatrix::DualMatrix(const BitString& key, const DualParams& params)
  : DenseMatrix(params.n, params.N()), key(key)
{
  PRF<BitString> prf(key);

  MULTI_TASK([this, &prf, &params](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      (*this->rows)[i] = prf(i, this->width);
    }
  }, params.n);
}

DualMatrix DualMatrix::sample(const DualParams& params) {
  return DualMatrix(BitString::sample(LAMBDA), params);
}

}
