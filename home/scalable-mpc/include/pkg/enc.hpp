#pragma once

#include "pkg/lpn.hpp"
#include "pkg/pcg.hpp"
#include "util/bitstring.hpp"
#include "util/params.hpp"
#include "util/random.hpp"

namespace LPN {

class Encryptor {
public:
  Encryptor(const EncryptionParams& params, Beaver::Triples* triples, bool decrypter = false)
    : ctx_size(ECC::CODEWORD_SIZE(params.msg_size)), params(params), decrypter(decrypter),
      triples(triples), A(ctx_size, params.key_size, params.pkey) { }

  BitString encrypt(const BitString& key, const BitString& message);
  BitString decrypt(const BitString& key, const BitString& ciphertext);

  const size_t ctx_size;
private:
  EncryptionParams params;

  // whether this party who is doing the decrypting
  bool decrypter;

  // triples used for error
  Beaver::Triples* triples;

  // public matrix
  DenseMatrix A;
};

}
