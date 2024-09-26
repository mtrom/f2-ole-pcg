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
  PCG(uint32_t id, const PCGParams& params) : id(id), params(params), ahe(params.primal.l) { }

  // non-interactive steps to prepare for the protocol
  void prepare();

  // interactive steps during the protocol
  void online(
    uint32_t other_id, Channel channel, RandomOTSender srots, RandomOTReceiver rrots
  );

  // non-interactive steps after online to finalize the output correlations
  std::pair<BitString, BitString> finalize(size_t other_id);

  // run the entire protocol with party `other_id`
  std::pair<BitString, BitString> run(
    uint32_t other_id, Channel channel, RandomOTSender srots, RandomOTReceiver rrots
  );

  // return the programmed inputs
  std::pair<BitString, BitString> inputs() const;

  // required number of oblivious transfers for one protocol run based on `params`
  std::pair<size_t, size_t> numOTs(uint32_t other_id) const;

protected:
  std::vector<AHE::Ciphertext> homomorphicInnerProduct(
    const std::vector<AHE::Ciphertext>& enc_s, bool sender
  ) const;

private:
  uint32_t id;
  PCGParams params;
  AHE ahe;

  // public matrices
  LPN::PrimalMatrix A;
  LPN::DualMatrix H;
  LPN::MatrixProduct B; // = AH

  // primal lpn secret vectors & errors
  BitString s0, s1;
  std::vector<uint32_t> e0, e1;

  // dual lpn error
  std::vector<uint32_t> epsilon;

  std::vector<AHE::Ciphertext> enc_s0, enc_s1;

  BitString send_masks, recv_masks;

  std::unique_ptr<EqTest> eqtester;

  // pprfs for both pcg directions
  PPRF send_eXs, recv_eXs;
  std::vector<DPF> send_eXas_eoe, recv_eXas_eoe;
  std::vector<DPF> send_eXas, recv_eXas;

  // TODO: why is this needed but send_eoe isn't?
  BitString recv_eoe, send_eoe;
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
