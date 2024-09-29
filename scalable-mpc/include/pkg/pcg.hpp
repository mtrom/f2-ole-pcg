#pragma once

#include <libscapi/include/comm/Comm.hpp>

#include "ahe/ahe.hpp"
#include "pkg/eqtest.hpp"
#include "pkg/lpn.hpp"
#include "pkg/pprf.hpp"
#include "pkg/rot.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/params.hpp"
#include "util/random.hpp"

namespace Beaver {

////////////////////////////////////////////////////////////////////////////////
// 2-PARTY CORRELATION OBJECTS
////////////////////////////////////////////////////////////////////////////////

class PCG {
public:
  PCG(const PCGParams& params) : params(params), ahe(params.primal.l) { }

  // run entire protocol
  BitString run(Channel channel, RandomOTSender srots, RandomOTReceiver rrots) {
    prepare();
    online(channel, srots, rrots);
    return finalize();
  };

  // non-interactive steps to prepare for the protocol
  virtual void prepare() = 0;

  // interactive steps during the protocol
  virtual void online(Channel channel, RandomOTSender srots, RandomOTReceiver rrots) = 0 ;

  // non-interactive steps after online to finalize the output correlations
  virtual BitString finalize() = 0;

  // return the programmed inputs
  BitString inputs() const;

  // required number of oblivious transfers for one protocol run based on `params`
  virtual std::pair<size_t, size_t> numOTs() const = 0;

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
  std::vector<DPF> eXas_eoe, eXas;
};

class Sender : public PCG {
public:
  Sender(const PCGParams& params) : PCG(params) { }
  void prepare() override;
  void online(Channel channel, RandomOTSender srots, RandomOTReceiver rrots) override;
  BitString finalize() override;
  std::pair<size_t, size_t> numOTs() const override;
};

class Receiver : public PCG {
public:
  Receiver(const PCGParams& params) : PCG(params) { }
  void prepare() override;
  void online(Channel channel, RandomOTSender srots, RandomOTReceiver rrots) override;
  BitString finalize() override;
  std::pair<size_t, size_t> numOTs() const override;

protected:
  // dual lpn error
  std::vector<uint32_t> epsilon;

  BitString eoe;
};

////////////////////////////////////////////////////////////////////////////////
// INTERFACE FOR GARBLED MPC
////////////////////////////////////////////////////////////////////////////////

class Triple {
public:
  bool a, b, c;

  Triple(bool a, bool b, bool c) : a(a), b(b), c(c) { }
};

class Triples {
public:
  Triples() : total_(0), a(0), b(0), c(0) { }
  Triples(BitString a, BitString b, BitString c) : total_(a.size()), a(a), b(b), c(c) { }

  // consume one triple
  Triple get();

  // reserve multiple triples
  Triples reserve(size_t n);

  // total number of triples in this object
  size_t total() const { return total_; }

  // number of triples remaining to be used
  size_t remaining() const { return total_ - used; }

protected:
  BitString a;
  BitString b;
  BitString c;

  size_t total_;
  size_t used = 0;
};

// transform from 2-party to n-party correlations using the transformation (and notation)
// from Appendix G of https://eprint.iacr.org/2019/448.pdf
Triples transform(
  BitString a, BitString b,                               // my programmed input
  std::vector<BitString> cij, std::vector<BitString> dji, // the 2-party correlations
  std::vector<BitString> seeds                            // a prf key shared between other party
);

////////////////////////////////////////////////////////////////////////////////
// MOCK CLASSES
////////////////////////////////////////////////////////////////////////////////

class MockSender {
public:
  MockSender(size_t size, BitString key) : prf(key), size(size) { }
  BitString run(BitString key) const;
  BitString lpnOutput() const;
protected:
  PRF<BitString> prf;
  size_t size;
};

class MockReceiver {
public:
  MockReceiver(size_t size, BitString key) : prf(key), size(size) { }
  BitString run(BitString key) const;
  BitString lpnOutput() const;
protected:
  PRF<BitString> prf;
  size_t size;
};

class MockPCG {
public:
  MockPCG(uint32_t id, size_t size);
  std::tuple<BitString, BitString, BitString> run(uint32_t id);
  std::pair<BitString, BitString> inputs() const;
private:
  uint32_t id;
  MockSender sender;
  MockReceiver receiver;
};

}
