#include "pkg/enc.hpp"

namespace LPN {

BitString Encryptor::encrypt(const BitString& key, const BitString& message) {
  BitString error(this->ctx_size);
  for (size_t i = 0; i < error.size(); i += 4) {
    Beaver::Triple t = triples->get();
    error[i + 0] = t.c;
    error[i + 1] = t.a ^ t.c;
    error[i + 2] = t.b ^ t.c;
    error[i + 3] = t.a ^ t.b ^ t.c ^ (decrypter ? 1 : 0);
  }
  return (A * key) ^ error ^ ECC::encode(message);
}

BitString Encryptor::decrypt(const BitString& key, const BitString& ciphertext) {
  return ECC::decode((A * key) ^ ciphertext);
}

}
