#pragma once

#include <libscapi/include/comm/Comm.hpp>

#include "pkg/lpn.hpp"
#include "pkg/pprf.hpp"
#include "pkg/rot.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"
#include "util/params.hpp"
#include "util/random.hpp"

namespace Beaver {

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
// 2-PARTY CORRELATION OBJECTS
////////////////////////////////////////////////////////////////////////////////

class Base {
public:
  Base(const PCGParams& params);

  // initalize the public lpn instances
  virtual void loadPublicMatrices(LPN::PrimalMatrix A, LPN::DualMatrix H, LPN::DenseMatrix B) {
    this->A = A;
    this->H = H;
    this->B = B;
  }

  // run the entire protocol and output the triples
  virtual BitString run(
    std::shared_ptr<CommParty> channel, RandomOTSender srots, RandomOTReceiver rrots
  ) const = 0;

  // get the output of our own lpn instance
  BitString lpnOutput() const;
protected:
  PCGParams params;

  // public matrices
  LPN::PrimalMatrix A;
  LPN::DualMatrix H;
  LPN::DenseMatrix B; // = AH

  // primal lpn secret vector & error
  BitString s;
  std::vector<uint32_t> e;

  // helper methods shared by both parties
  BitString secretTensorProcessing(std::vector<PPRF> pprfs) const;

  // compute the ⟨aᵢ,s⟩ · e terms
  BitString sendInnerProductTerm(
    std::shared_ptr<CommParty> channel, RandomOTSender rots
  ) const;

  BitString receiveInnerProductTerm(
    std::shared_ptr<CommParty> channel, RandomOTReceiver rots
  ) const;
};

class Sender : public Base {
public:
  Sender(const PCGParams& params);

  Sender(
    const PCGParams& params, LPN::PrimalMatrix A, LPN::DualMatrix H, LPN::DenseMatrix B
  ) : Sender(params) {
    this->loadPublicMatrices(A, H, B);
  }

  BitString run(
    std::shared_ptr<CommParty> channel, RandomOTSender srots, RandomOTReceiver rrots
  ) const override;
protected:
  // compute the ⟨bᵢ⊗ aᵢ,e₁ ⊗ s⟩ term
  BitString secretTensor(std::shared_ptr<CommParty> channel, RandomOTSender rots) const;

  // compute the (e₀ ○ e₁) term
  BitString errorProduct(std::shared_ptr<CommParty> channel, RandomOTSender rots) const;
};

class Receiver : public Base {
public:
  Receiver(const PCGParams& params);
  Receiver(
    const PCGParams& params, LPN::PrimalMatrix A, LPN::DualMatrix H, LPN::DenseMatrix B
  ) : Receiver(params) {
    this->loadPublicMatrices(A, H, B);
  }

  void loadPublicMatrices(LPN::PrimalMatrix A, LPN::DualMatrix H, LPN::DenseMatrix B) override;

  BitString run(
    std::shared_ptr<CommParty> channel, RandomOTSender srots, RandomOTReceiver rrots
  ) const override;
protected:
  // dual lpn error
  std::vector<uint32_t> epsilon;

  // compute the ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩
  BitString secretTensor(std::shared_ptr<CommParty> channel, RandomOTReceiver rots) const;

  // compute the (e₀ ○ e₁) term
  BitString errorProduct(std::shared_ptr<CommParty> channel, RandomOTReceiver rots) const;
};

class PCG {
public:
  PCG(uint32_t id, const PCGParams& params);

  // run the pcg protocol with party `id` both has a sender and receiver
  std::pair<BitString, BitString> run(
    uint32_t id, std::shared_ptr<CommParty> channel, RandomOTSender srots, RandomOTReceiver rrots
  ) const;

  // return the programmed inputs (as both sender and receiver)
  std::pair<BitString, BitString> inputs() const;

  // required number of oblivious transfers based on the given parameters
  size_t numOTs() const;
private:
  uint32_t id;
  PCGParams params;

  Sender sender;
  Receiver receiver;
};

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
