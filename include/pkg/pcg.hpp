#pragma once

#include "ahe/ahe.hpp"
#include "pkg/eqtest.hpp"
#include "pkg/lpn.hpp"
#include "pkg/pprf.hpp"
#include "pkg/rot.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/params.hpp"
#include "util/random.hpp"

namespace PCG {

class Base {
public:
  Base(const PCGParams& params) : params(params), ahe(params.primal.l) { }

  // run entire protocol
  BitString run(Channel channel, ROT::Sender srots, ROT::Receiver rrots) {
    init();
    prepare();
    online(channel, srots, rrots);
    finalize();
    expand();
    return output;
  };

  // initialize / clear public information
  void init();
  void clear() { A = LPN::PrimalMatrix(); H = LPN::DualMatrix(); B = LPN::MatrixProduct(); };

  // non-interactive steps to prepare for the protocol
  virtual void prepare() = 0;

  // interactive steps during the protocol
  virtual void online(Channel channel, ROT::Sender srots, ROT::Receiver rrots) = 0;

  // non-interactive steps after online to prepare to output correlations
  virtual void finalize() = 0;

  // generate the actual correlations
  void expand();

  // return the programmed inputs
  BitString inputs() const;

  // required number of oblivious transfers for one protocol run based on `params`
  virtual std::pair<size_t, size_t> numOTs() const = 0;

  // output correlations
  BitString output;
protected:
  std::vector<AHE::Ciphertext> homomorphicInnerProduct(
    const std::vector<AHE::Ciphertext>& enc_s
  ) const;

  PCGParams params;
  AHE ahe;

  // public matrices
  LPN::PrimalMatrix A;
  LPN::DualMatrix H;
  LPN::MatrixProduct B; // = AH

  // primal lpn secret vectors & errors
  BitString s;
  std::vector<uint32_t> e;

  // ciphertext of secret vector
  std::vector<AHE::Ciphertext> enc_s;

  // random mask nonces
  BitString masks;

  // protocol pprfs
  std::vector<PPRF> eXs;
  std::vector<BitPPRF> eXas_eoe, eXas;

  // transpose of (ε ⊗ s) matrix
  std::vector<BitString> eXs_matrix;
};

class Sender : public Base {
public:
  Sender(const PCGParams& params) : Base(params) { }
  void prepare() override;
  void online(Channel channel, ROT::Sender srots, ROT::Receiver rrots) override;
  void finalize() override;
  std::pair<size_t, size_t> numOTs() const override;
};

class Receiver : public Base {
public:
  Receiver(const PCGParams& params) : Base(params) { }
  void prepare() override;
  void online(Channel channel, ROT::Sender srots, ROT::Receiver rrots) override;
  void finalize() override;
  std::pair<size_t, size_t> numOTs() const override;

protected:
  // dual lpn error
  std::vector<uint32_t> epsilon;

  BitString eoe;
};

}
