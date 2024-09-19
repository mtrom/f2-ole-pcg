#include "pkg/lpn.hpp"

#include <algorithm>
#include <iostream>
#include <thread>

#include "util/concurrency.hpp"
#include "util/defines.hpp"
#include "util/random.hpp"

namespace LPN {

////////////////////////////////////////////////////////////////////////////////
// MATRIX FOR LPN ENCRYPTION
////////////////////////////////////////////////////////////////////////////////

EncryptionMatrix::EncryptionMatrix(size_t height, size_t width, const BitString& key)
  : width(width), rows(std::make_shared<std::vector<BitString>>(height))
{
  PRF<BitString> prf(key);
  for (size_t i = 0; i < height; i++) {
    (*this->rows)[i] = prf(i, width);
  }
}

bool EncryptionMatrix::operator[](std::pair<size_t, size_t> idx) const {
  if (idx.first >= (*this->rows).size() || idx.second >= this->width) {
    throw std::domain_error("[EncryptionMatrix::operator[](std::pair)] idx out of range");
  }
  return (*rows)[idx.first][idx.second];
}

BitString EncryptionMatrix::operator[](size_t idx) const {
  if (idx >= (*this->rows).size()) {
    throw std::domain_error("[EncryptionMatrix::operator[](size_t)] idx out of range");
  }
  return (*this->rows)[idx];
}

std::pair<size_t, size_t> EncryptionMatrix::dim() const {
  return std::make_pair((*this->rows).size(), this->width);
}

BitString EncryptionMatrix::operator*(const BitString& other) const {
  if (this->dim().second != other.size()) {
    throw std::domain_error(
      "[EncryptionMatrix::operator*(BitString)] vector dimension mismatched"
    );
  }
  BitString result(this->dim().first);
  for (size_t i = 0; i < (*this->rows).size(); i++) {
    result[i] = (*this->rows)[i] * other;
  }
  return result;
}

std::string EncryptionMatrix::toString() const {
  std::string out;
  for (size_t i = 0; i < (*this->rows).size(); i++) {
    out += (*this->rows)[i].toString() + "\n";
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// PRIMAL MATRIX
////////////////////////////////////////////////////////////////////////////////

std::set<uint32_t> PrimalMatrix::getNonZeroElements(size_t idx) const {
  std::set<uint32_t> points;
  for (size_t i = 0; points.size() < this->sparsity; i++) {
    points.insert((*this->prf)(std::make_pair(idx, i), this->width));
  }
  return points;
}

std::pair<size_t, size_t> PrimalMatrix::dim() const {
  return std::make_pair(this->height, this->width);
}

BitString PrimalMatrix::operator*(const BitString& other) const {
  if (this->dim().second != other.size()) {
    throw std::domain_error("[PrimalMatrix::operator*(BitString)] vector dimension mismatched");
  }
  BitString result(this->dim().first);
  for (size_t i = 0; i < result.size(); i++) {
    for (uint32_t point : this->getNonZeroElements(i)) {
      if (other[point]) { result[i] ^= true; }
    }
  }
  return result;
}

MatrixProduct PrimalMatrix::operator*(const DualMatrix& other) const {
  if (this->dim().second != other.dim().first) {
    throw std::domain_error("[PrimalMatrix::operator*] matrix dimensions mismatched");
  }
  return MatrixProduct(*this, other);
}

BitString PrimalMatrix::operator[](size_t idx) const {
  if (idx >= this->height) {
    throw std::domain_error("[SparseMatrix::operator[](size_t)] idx out of range");
  }

  BitString row(this->width);
  for (uint32_t i : this->getNonZeroElements(idx)) { row[i] = true; }
  return row;
}

std::string PrimalMatrix::toString() const {
  std::string out;
  for (size_t i = 0; i < this->height; i++) {
    out += this->operator[](i).toString() + "\n";
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// DUAL MATRIX
////////////////////////////////////////////////////////////////////////////////

bool DualMatrix::operator[](std::pair<size_t, size_t> idx) const {
  // each row is organized by blocks of size LAMBDA
  BitString block = (*this->prf)(std::make_pair(idx.first, idx.second / LAMBDA), LAMBDA);

  // to get the ith bit in the row, get the (i % LAMBDA) bit in the (i / LAMBDA) block
  return block[idx.second % LAMBDA];
}

////////////////////////////////////////////////////////////////////////////////
// MATRIX PRODUCT
////////////////////////////////////////////////////////////////////////////////

BitString MatrixProduct::operator[](size_t idx) const {
  if (idx >= this->dim().first) {
    throw std::domain_error("[MatrixProduct::operator[](size_t)] idx out of range");
  }

  BitString row(this->dim().second);

  for (size_t col = 0; col < row.size(); col++) {
    bool element = false;
    for (uint32_t point : primal.getNonZeroElements(idx)) {
      if (dual[{point, col}]) { element = !element; }
    }
    row[col] = element;
  }
  return row;
}

}
