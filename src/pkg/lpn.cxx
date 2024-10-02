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
  return std::find(
    (*this->points)[idx.first].begin(), (*this->points)[idx.first].end(), idx.second
  ) != (*this->points)[idx.first].end();
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

MatrixProduct SparseMatrix::operator*(const DenseMatrix& other) const {
  if (this->dim().second != other.dim().first) {
    throw std::domain_error(
      "[SparseMatrix::operator*(DenseMatrix)] matrix dimensions mismatched"
    );
  }
  return MatrixProduct(*this, other);
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
        uint32_t point = prf(std::make_pair(i, j), params.k);
        if (
          std::find(points->at(i).begin(), points->at(i).end(), point) == points->at(i).end()
        ) {
          (*this->points)[i].push_back(point);
        }
      }
      std::sort((*this->points)[i].begin(), (*this->points)[i].end());
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

  MULTI_TASK([this, &prf](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      (*this->rows)[i] = prf(i, this->width);
    }
  }, this->rows->size());
}

DualMatrix DualMatrix::sample(const DualParams& params) {
  return DualMatrix(BitString::sample(LAMBDA), params);
}

////////////////////////////////////////////////////////////////////////////////
// MATRIX PRODUCT
////////////////////////////////////////////////////////////////////////////////

BitString MatrixProduct::operator[](size_t idx) const {
  if (idx >= this->dim().first) {
    throw std::domain_error("[MatrixProduct::operator[](size_t)] idx out of range");
  }

  BitString row(this->dim().second);

  for (uint32_t point : sparse.getNonZeroElements(idx)) {
    row ^= dense[point];
  }
  return row;
}

}
