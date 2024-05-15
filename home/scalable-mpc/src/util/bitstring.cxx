#include "util/bitstring.hpp"

#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <tuple>

#include <openssl/bn.h>
#include <openssl/evp.h>

////////////////////////////////////////////////////////////////////////////////
// SERIALIZE / DESERIALIZE OPERATORS
////////////////////////////////////////////////////////////////////////////////

BitString BitString::fromUInt(const uint32_t& value, size_t bits) {
  if (bits > 32) {
    throw std::out_of_range("[BitString::fromUInt] there are only 32 bits in a uint32_t");
  }
  std::vector<unsigned char> bytes;

  for (auto i = 0; i < (bits + 7) / 8; i++) {
    bytes.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xFF));
  }

  return BitString(bytes, bits);
}

uint32_t BitString::toUInt() const {
  size_t i = 0;
  uint32_t value = 0;
  for (size_t b = 0; b < this->size() && b < 32; b++, i++) {
    value |= (this->operator[](i) << b);
  }
  return value;
}

std::vector<uint32_t> BitString::toUInts(size_t bits) const {
  if (bits > 32) {
    throw std::out_of_range("[BitString::toUInts()] there are only 32 bits in a uint32_t");
  } else if (size_ % bits != 0) {
    throw std::domain_error(
        "[BitString::toUInts()] " + std::to_string(size_) + " \% "
        + std::to_string(bits) + " != 0"
    );
  }

  std::vector<uint32_t> out;
  size_t i = 0;
  while (i < size_) {
    uint32_t value = 0;
    for (size_t b = 0; b < bits; b++, i++) {
      value |= (this->operator[](i) << b);
    }
    out.push_back(value);
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// COMPARISON OPERATORS
////////////////////////////////////////////////////////////////////////////////

bool BitString::operator==(const BitString& other) const {
  if (this->size() != other.size()) { return false; }
  for (size_t i = 0; i < this->size(); i++) {
    if (this->operator[](i) != other[i]) { return false; }
  }
  return true;
}

bool BitString::operator!=(const BitString& other) const {
  return !this->operator==(other);
}

////////////////////////////////////////////////////////////////////////////////
// ACCESS OPERATORS
////////////////////////////////////////////////////////////////////////////////

BitString& BitString::operator=(const BitString& other) {
  if (this != &other) {
    bytes = other.bytes;
    size_ = other.size_;
  }
  return *this;
}

const bool BitString::operator[](size_t i) const {
  if (i >= size_) {
    throw std::out_of_range(
        "[BitString::operator[](size_t)]" + std::to_string(i) + " is out of range"
    );
  }
  return ((bytes[i / 8] >> (i % 8)) & 0x01);
}

BitString BitString::operator[](std::pair<size_t, size_t> range) const {
  size_t from, to;
  std::tie(from, to) = range;

  if (from >= size_) {
    throw std::out_of_range(
      "[BitString::operator[](std::pair)] from=" + std::to_string(from) + " is out of range"
    );
  } else if (to > size_) {
    throw std::out_of_range(
      "[BitString::operator[](std::pair)] to=" + std::to_string(to) + " is out of range"
    );
  } else if (from > to) {
    throw std::invalid_argument(
      "[BitString::operator[](std::pair)] invalid range ("
      + std::to_string(from) + " > " + std::to_string(to) + ")"
    );
  }

  BitString substring(to - from);
  for (size_t i = from; i < to; i++) {
    substring[i - from] = this->operator[](i);
  }
  return substring;

}

////////////////////////////////////////////////////////////////////////////////
// CONCATENATION OPERATORS
////////////////////////////////////////////////////////////////////////////////

// TODO: just append in the special case where bytes.size() * 8 == size_
BitString& BitString::operator+=(const BitString& other) {
  this->size_ += other.size_;
  bytes.resize((this->size_ + 7) / 8);

  for (size_t i = 0; i < other.size_; i++) {
    this->operator[](i + this->size_ - other.size_) = other[i];
  }

  return *this;
}

BitString BitString::operator+(const BitString& other) const {
  BitString result(*this);
  result += other;
  return result;
}

BitString& BitString::operator+=(const bool& bit) {
  this->size_++;
  bytes.resize((this->size_ + 7) / 8);
  this->operator[](this->size_ - 1) = bit;
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
// BITWISE OPERATORS
////////////////////////////////////////////////////////////////////////////////

BitString& BitString::operator^=(const BitString& other) {
  if (other.size() != this->size()) {
    throw std::domain_error("[BitString::operator^=] size mismatch ("
        + std::to_string(other.size()) + " vs. " + std::to_string(this->size()) + ")");
  }
  for (size_t i = 0; i < this->bytes.size(); i++) {
    this->bytes[i] = this->bytes[i] ^ other.bytes[i];
  }
  return *this;
}

BitString BitString::operator^(const BitString& other) const {
  BitString result(*this);
  result ^= other;
  return result;
}

BitString& BitString::operator&=(const BitString& other) {
  if (other.size() != this->size()) {
    throw std::domain_error("[BitString::operator&=] size mismatch");
  }
  for (size_t i = 0; i < this->bytes.size(); i++) {
    this->bytes[i] = this->bytes[i] & other.bytes[i];
  }
  return *this;
}

BitString BitString::operator&(const BitString& other) const {
  BitString result(*this);
  result &= other;
  return result;
}

BitString& BitString::operator|=(const BitString& other) {
  if (other.size() != this->size()) {
    throw std::domain_error("[BitString::operator|=] size mismatch");
  }
  for (size_t i = 0; i < this->bytes.size(); i++) {
    this->bytes[i] = this->bytes[i] | other.bytes[i];
  }
  return *this;
}

BitString BitString::operator|(const BitString& other) const {
  BitString result(*this);
  result |= other;
  return result;
}

BitString BitString::operator~() const {
  BitString result(*this);
  for (size_t i = 0; i < result.nBytes(); i++) {
    result.bytes[i] = ~result.bytes[i];
  }
  return result;
}

bool BitString::operator*(const BitString& other) const {
  if (other.size() != this->size()) {
    throw std::domain_error("[BitString::operator*] size mismatch");
  }
  BitString result(*this);
  result &= other;
  return (result.weight() % 2 != 0);
}

BitString BitString::tensor(const BitString& other) const {
  BitString result;
  for (size_t i = 0; i < this->size(); i++) {
    result += this->operator[](i) ? other : BitString(other.size());
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// ERROR CORRECTING CODE
////////////////////////////////////////////////////////////////////////////////

BitString ECC::encode(const BitString& message) {
  BitString out(ECC::CODEWORD_SIZE(message.size()));

  for (size_t i = 0, o = 0; i < message.size(); i++) {
    if (i % 2 == 0) {
      out[o] = message[i];
      out[o + 1] = message[i];
      out[o + 2] = message[i];
    } else {
      out[o + 3] = message[i];
      o += 4;
    }
  }

  return out;
}

BitString ECC::decode(const BitString& message) {
  BitString out(message.size() / 2);

  for (size_t i = 0, o = 0; i < message.size(); i += 4, o += 2) {
    if (message[i] == message[i + 1] && message[i + 1] == message[i + 2]) {
      out[o] = message[i];
      out[o + 1] = !message[i + 3];
    } else if (message[i] == message[i + 1] || message[i] == message[i + 2]) {
      out[o] = message[i];
      out[o + 1] = message[i + 3];
    } else {
      out[o] = message[i + 1];
      out[o + 1] = message[i + 3];
    }
  }

  return out;
}

size_t ECC::CODEWORD_SIZE(const size_t& msg_size) {
  return (msg_size & 0x01) ? 2 * (msg_size + 1) : 2 * msg_size;
}

////////////////////////////////////////////////////////////////////////////////
size_t BitString::weight() const {
  size_t w = 0;
  for (size_t i = 0; i < size(); i++) {
    if (this->operator[](i)) { w++; }
  }
  return w;
}


BitString BitString::reverse() const {
  BitString out(this->size());
  for (size_t i = 0; i < this->size(); i++) {
    out[i] = this->operator[](this->size() - 1 - i);
  }
  return out;
}

BitString BitString::sample(size_t size) {
  // generate the output
  BIGNUM* bits = BN_new();
  BN_rand(bits, size, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);

  // convert to bits
  std::vector<unsigned char> bytes((size + 7) / 8);

  BN_bn2bin(bits, bytes.data());

  // free used memory
  BN_clear_free(bits);

  return BitString(bytes, size);
}


std::vector<unsigned char> BitString::expand() {
  std::vector<unsigned char> out;
  for (size_t i = 0; i < size_; i++) {
    out.push_back(this->operator[](i));
  }
  return out;
}

BitString BitString::aes(size_t size) {
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr) {
    throw std::runtime_error("[BitString::aes] EVP_CIPHER_CTX_new error");
  }

  std::vector<unsigned char> key(this->bytes);

  // ensure the key is large enough
  const size_t BLOCK_SIZE = 16;
  key.resize(BLOCK_SIZE);

  // use AES encryption in CTR mode
  if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, key.data(), nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("[BitString::aes] EVP_EncryptInit_ex2 error");
  }

  // output is initially sized to the minimum multiple of the block size we need
  const size_t blocks = (((size + 7) / 8) + BLOCK_SIZE - 1) / BLOCK_SIZE;
  std::vector<unsigned char> output(blocks * BLOCK_SIZE);

  // run aes until we have enough pseudorandom bits
  int outl;
  std::vector<unsigned char> input(BLOCK_SIZE, 0);
  for (size_t i = 0; i < output.size(); i += outl) {
    if (EVP_EncryptUpdate(ctx, &output[i], &outl, input.data(), input.size()) != 1) {
      EVP_CIPHER_CTX_free(ctx);
      throw std::runtime_error("[BitString::aes] EVP_EncryptUpdate error");
    }
  }

  EVP_CIPHER_CTX_free(ctx);

  // truncate to the number of bytes we actually need
  output.resize((size + 7) / 8);

  // ensure any extra bits are 0 (for internal consistancy)
  output[output.size() - 1] &= (0xFF >> ((output.size() * 8) - size));

  return BitString(output, size);
}

BitString::BitString(const std::string& str) : bytes((str.size() + 7) / 8), size_(str.size()) {
  for (size_t i = 0; i < str.size(); i++) {
    if (str[i] == '1') {
      this->operator[](i) = true;
    } else if (str[i] != '0') {
      throw std::invalid_argument("[BitString(std::string)] invalid character: " + str[i]);
    }
  }
}

std::string BitString::toString() const {
  std::string out = "";
  for (size_t i = 0; i < size(); i++) {
    if (this->operator[](i)) {
      out += "1";
    } else {
      out += "0";
    }
  }
  return out;
}

std::string BitString::toHexString() const {
  std::ostringstream out;
  out << std::hex << std::setfill('0');

  for (unsigned char byte : this->bytes) {
    out << std::setw(2) << static_cast<int>(byte);
  }

  return out.str();
}

std::ostream& operator<<(std::ostream& os, const BitString& bs) {
  os << (bs.size_ > 32 ? bs.toHexString() : bs.toString());
  return os;
}
