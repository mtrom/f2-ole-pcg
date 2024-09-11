#pragma once

#include <stdexcept>
#include <string>
#include <vector>

class BitString {
protected:
  std::vector<unsigned char> bytes;
  size_t size_;
private:
  // private class to allow for bit setting
  class BitReference {
  public:
    BitReference(std::vector<unsigned char>& bytes, size_t pos) : bytes_(bytes), pos_(pos) { }

    BitReference& operator=(bool value) {
      if (value) {
        bytes_[pos_ / 8] |= (1 << (pos_ % 8));   // Set the bit
      } else {
        bytes_[pos_ / 8] &= ~(1 << (pos_ % 8));  // Clear the bit
      }
      return *this;
    }

    BitReference& operator=(BitReference& value) {
      this->operator=(bool(value));
      return *this;
    }

    BitReference& operator^=(bool other) {
      bool ours = (bytes_[pos_ / 8] >> (pos_ % 8)) & 0x01;
      this->operator=(ours ^ other);
      return *this;
    }

    BitReference& operator&=(bool other) {
      bool ours = (bytes_[pos_ / 8] >> (pos_ % 8)) & 0x01;
      this->operator=(ours & other);
      return *this;
    }

    BitReference& operator|=(bool other) {
      bool ours = (bytes_[pos_ / 8] >> (pos_ % 8)) & 0x01;
      this->operator=(ours | other);
      return *this;
    }

    operator bool() const {
      return (bytes_[pos_ / 8] >> (pos_ % 8)) & 0x01;
    }

  private:
    std::vector<unsigned char>& bytes_;
    size_t pos_;
  };
public:
  BitString() : bytes(0), size_(0) { }
  BitString(size_t size) : bytes((size + 7) / 8), size_(size) { }
  BitString(std::vector<unsigned char> bytes) : bytes(bytes), size_(bytes.size() * 8) { }
  BitString(std::vector<unsigned char> bytes, size_t size) : bytes(bytes), size_(size) { }
  BitString(const BitString& other) : bytes(other.bytes), size_(other.size_) { }
  BitString(unsigned char* bytes, size_t size)
    : bytes(bytes, bytes + ((size + 7) / 8)), size_(size) { }

  // serialize and deserialize
  static BitString fromUInt(const uint32_t& value, size_t bits = 32);
  uint32_t toUInt() const;
  std::vector<uint32_t> toUInts(size_t bits = 32) const;

  // allows variable assignment
  BitString& operator=(const BitString& other);

  // get & set the ith bit
  const bool operator[](size_t i) const;
  BitReference operator[](size_t i) {
    if (i >= size_) {
      throw std::out_of_range("[BitString] " + std::to_string(i) + " is out of range");
    }
    return BitReference(bytes, i);
  }

  // get a substring
  BitString operator[](std::pair<size_t, size_t> range) const;

  // concatenation operators
  BitString& operator+=(const BitString& other);
  BitString  operator+ (const BitString& other) const;
  BitString& operator+=(const bool& bit);
  static BitString concat(const std::vector<BitString> in);

  // comparison operators
  bool operator==(const BitString& other) const;
  bool operator!=(const BitString& other) const;
  bool operator<(const BitString& other) const;

  // bitwise operators manipulating the underlying bytes
  BitString& operator^=(const BitString& other);
  BitString  operator^ (const BitString& other) const;
  BitString& operator&=(const BitString& other);
  BitString  operator& (const BitString& other) const;
  BitString& operator|=(const BitString& other);
  BitString  operator| (const BitString& other) const;
  BitString  operator~ () const;

  // bitwise inner product
  bool operator*(const BitString& other) const;

  // tensor product (for testing)
  BitString tensor(const BitString& other) const;

  // to mirror vector access
  size_t size() const { return size_; }
  unsigned char* data() { return bytes.data(); }
  std::vector<unsigned char>::iterator begin() { return bytes.begin(); }
  std::vector<unsigned char>::iterator end() { return bytes.end(); }

  size_t nBytes() const { return bytes.size(); }
  std::vector<unsigned char> toBytes() const { return bytes; }

  // gets hamming weight of the bit string
  size_t weight() const;

  // create a bitstring that has reversed bits
  BitString reverse() const;

  // uniformly sample `size` bits
  static BitString sample(size_t size);

  // conver to a byte vector where each bit is expanded to a whole byte
  std::vector<unsigned char> expand();

  // using this as a key, expand to `size` bits using aes
  BitString aes(size_t size);

  // for testing & debugging purposes
  BitString(const std::string& str);
  std::string toString() const;
  std::string toHexString() const;
  friend std::ostream& operator<<(std::ostream& os, const BitString& bs);
};

std::ostream& operator<<(std::ostream& os, const BitString& bs);

// our error correcting code for lpn encryption
namespace ECC {
BitString encode(const BitString& message);
BitString decode(const BitString& message);

size_t CODEWORD_SIZE(const size_t& msg_size);
}
