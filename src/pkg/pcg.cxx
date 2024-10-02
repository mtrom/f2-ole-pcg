#include "pkg/pcg.hpp"

#include <cmath>
#include <iostream>
#include <thread>

#include "ahe/ahe.hpp"
#include "pkg/eqtest.hpp"
#include "pkg/pprf.hpp"
#include "util/concurrency.hpp"
#include "util/defines.hpp"
#include "util/transpose.hpp"

namespace PCG {

////////////////////////////////////////////////////////////////////////////////
// PCG PROTOCOL METHODS
////////////////////////////////////////////////////////////////////////////////

void Base::init() {
  this->A = LPN::PrimalMatrix(params.pkey, params.primal);
  this->H = LPN::DualMatrix(params.dkey, params.dual);
  this->B = LPN::MatrixProduct(A, H);
}

void Sender::prepare() {

  // initialize the pprfs that we are sending
  this->eXs = PPRF::sample(
    params.dual.t, LAMBDA, params.primal.k, params.dual.blockSize()
  );
  this->eXas_eoe = BitPPRF::sample(params.primal.t, LAMBDA, params.primal.blockSize());

  // sample primal error vectors
  this->e = sampleVector(params.primal.t, params.primal.blockSize());

  // sample primal secret vector
  this->s = BitString::sample(params.primal.k);

  // encrypt secret vector
  this->enc_s = this->ahe.encrypt(this->s);

  // masks for (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) terms
  this->masks = BitString::sample(this->params.primal.t);
}

void Receiver::prepare() {

  // initialize the pprfs that we are sending
  this->eXas = BitPPRF::sample(params.primal.t, LAMBDA, params.primal.blockSize());

  // sample primal error vectors
  this->e = sampleVector(params.primal.t, params.primal.blockSize());

  // sample dual error vector
  this->epsilon = sampleVector(params.dual.t, params.dual.blockSize());

  // compute secret vector based on the dual matrix H and error ε
  this->s = BitString(params.primal.k);
  for (size_t i = 0; i < params.primal.k; i++) {
    for (size_t j = 0 ; j < params.dual.t; j++) {
      if (this->H[{i, j * params.dual.blockSize() + this->epsilon[j]}]) {
        this->s[i] ^= 1;
      }
    }
  }

  // encrypt secret vector
  this->enc_s = this->ahe.encrypt(this->s);

  // masks for (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) terms
  this->masks = BitString::sample(this->params.primal.t);
}

void Sender::online(Channel channel, ROT::Sender srots, ROT::Receiver rrots) {

  // equality test for (e₀ ○ e₁) terms
  BitString eoe = EqTestSender(
    params.primal.errorBits(), params.eqTestThreshold, params.primal.t, channel, srots
  ).run(this->e);

  // exchange encrypted secret vectors Enc(s₀) and Enc(s₁)
  this->ahe.send(this->enc_s, channel, true);
  this->enc_s.clear();
  std::vector<AHE::Ciphertext> other_enc_s = this->ahe.receive(
    this->params.primal.k, channel, true
  );

  // homomorphically compute Enc(⟨aᵢ,s₁⟩)
  std::vector<AHE::Ciphertext> enc_eXas = homomorphicInnerProduct(other_enc_s);
  other_enc_s.clear();

  // swap Enc(⟨aᵢ,s₀⟩) and Enc(⟨aᵢ,s₁⟩)
  this->ahe.send(enc_eXas, channel);
  std::vector<AHE::Ciphertext> resp = this->ahe.receive(this->params.primal.t, channel);

  BitString decrypted_resp = this->ahe.decrypt(resp);
  resp.clear();

  // exchange all pprfs
  BitPPRF::send(this->eXas_eoe, decrypted_resp ^ eoe, channel, srots);
  this->eXas = BitPPRF::receive(
    this->e, LAMBDA, params.primal.blockSize(), channel, rrots
  );
  PPRF::send(this->eXs, this->s, channel, srots);
}

void Receiver::online(Channel channel, ROT::Sender srots, ROT::Receiver rrots) {

  // equality test for (e₀ ○ e₁) terms
  this->eoe = EqTestReceiver(
    params.primal.errorBits(), params.eqTestThreshold, params.primal.t, channel, rrots
  ).run(this->e);

  // exchange encrypted secret vectors Enc(s₀) and Enc(s₁)
  std::vector<AHE::Ciphertext> other_enc_s = this->ahe.receive(
    this->params.primal.k, channel, true
  );
  this->ahe.send(this->enc_s, channel, true);
  this->enc_s.clear();

  // homomorphically compute Enc(⟨aᵢ,s₀⟩)
  std::vector<AHE::Ciphertext> enc_eXas = homomorphicInnerProduct(other_enc_s);
  other_enc_s.clear();

  // swap Enc(⟨aᵢ,s₀⟩) and Enc(⟨aᵢ,s₁⟩)
  std::vector<AHE::Ciphertext> resp = this->ahe.receive(this->params.primal.t, channel);
  this->ahe.send(enc_eXas, channel);

  BitString decrypted_resp = this->ahe.decrypt(resp);
  resp.clear();

  // exchange pprfs
  this->eXas_eoe = BitPPRF::receive(
    this->e, LAMBDA, params.primal.blockSize(), channel, rrots
  );
  BitPPRF::send(this->eXas, decrypted_resp, channel, srots);
  this->eXs = PPRF::receive(
    this->epsilon, LAMBDA, params.primal.k, params.dual.blockSize(), channel, rrots
  );
}

void Sender::finalize() {

  // arrange our shares of the (ε ⊗ s) matrix by column
  this->eXs_matrix = transpose(this->eXs, params);

  // expand the pprf that was received
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->eXas[i].expand();
    }
  }, params.primal.t);

  // concatenate the image for each error block to get final output
  this->output.clear();
  for (size_t i = 0; i < params.primal.t; i++) {
    BitString image = this->eXas_eoe[i].image() ^ this->eXas[i].image();

    // at our error position, xor with our shares of (e₀ ○ e₁)
    image[this->e[i]] ^= this->masks[i];

    this->output += image;
  }
  if (this->output.size() != params.size) { this->output.resize(params.size); }

  // free up memory
  for (BitPPRF& pprf : this->eXas_eoe) { pprf.clear(); }
  for (BitPPRF& pprf : this->eXas)     { pprf.clear(); }
}

void Receiver::finalize() {

  // expand the (ε ⊗ s) pprf
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->eXs[i].expand();
    }
  }, this->eXs.size());

  // arrange our shares of the (ε ⊗ s) matrix by column
  this->eXs_matrix = transpose(this->eXs, params);

  // expand the (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) pprf
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->eXas_eoe[i].expand();
    }
  }, params.primal.t);

  // in each direction, concatenate the image for each error block to get final output
  this->output.clear();
  for (size_t i = 0; i < params.blocks(); i++) {
    BitString image = this->eXas_eoe[i].image() ^ this->eXas[i].image();

    // at our error position, xor with our shares of (e₀ ○ e₁)
    image[this->e[i]] ^= this->masks[i] ^ this->eoe[i];

    this->output += image;
  }
  if (this->output.size() != params.size) { this->output.resize(params.size); }

  // free up memory
  for (BitPPRF& pprf : this->eXas_eoe) { pprf.clear(); }
  for (BitPPRF& pprf : this->eXas)     { pprf.clear(); }
}

void Base::expand() {
  // compute shares of the ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ vector
  auto baex = TASK_REDUCE<BitString>([this](size_t start, size_t end) {
    BitString out(end - start);
    for (size_t i = start; i < end; i++) {
      BitString aXeXs(params.dual.N());
      for (uint32_t idx : this->A.getNonZeroElements(i)) {
        aXeXs ^= this->eXs_matrix[idx];
      }
      out[i - start] = this->B[i] * aXeXs;
    }
    return out;
  }, [](std::vector<BitString> results) {
    BitString out;
    for (const BitString& result : results) {
      out += result;
    }
    return out;
  }, params.size);

  this->output ^= baex;
}

std::vector<AHE::Ciphertext> Base::homomorphicInnerProduct(
  const std::vector<AHE::Ciphertext>& enc_s
) const {
  std::vector<AHE::Ciphertext> out;
  for (size_t i = 0; i < params.primal.t; i++) {
    uint32_t idx = (i * this->params.primal.blockSize()) + this->e[i];

    // homomorphically compute the inner product of aᵢand Enc(s)
    std::vector<uint32_t> points = this->A.getNonZeroElements(idx);
    AHE::Ciphertext ctx = enc_s[points[0]];
    for (size_t i = 1; i < points.size(); i++) {
      ctx = this->ahe.add(ctx, enc_s[points[i]]);
    }

    // add our share to get their share
    ctx = this->ahe.add(ctx, this->masks[i]);

    out.push_back(ctx);
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// HELPER METHODS
////////////////////////////////////////////////////////////////////////////////

std::pair<size_t, size_t> Sender::numOTs() const {
  return std::make_pair(
    (
      this->params.dual.t * ((size_t) ceil(log2(this->params.dual.blockSize())) + 1)
      + this->params.primal.t * ((size_t) ceil(log2(this->params.primal.blockSize())))
      + EqTest::numOTs(
        this->params.primal.errorBits(), this->params.eqTestThreshold, this->params.primal.t
      )
    ), (
      this->params.primal.t * ((size_t) ceil(log2(this->params.primal.blockSize())))
    )
  );
}

std::pair<size_t, size_t> Receiver::numOTs() const {
  return std::make_pair(
    (
      this->params.primal.t * ((size_t) ceil(log2(this->params.primal.blockSize())))
    ), (
      this->params.dual.t * ((size_t) ceil(log2(this->params.dual.blockSize())) + 1)
      + this->params.primal.t * ((size_t) ceil(log2(this->params.primal.blockSize())))
      + EqTest::numOTs(
        this->params.primal.errorBits(), this->params.eqTestThreshold, this->params.primal.t
      )
    )
  );
}

// the programmed inputs are just the output of the lpn instances
BitString Base::inputs() const {
  BitString out = this->A * this->s;
  for (size_t i = 0; i < params.primal.t; i++) {
    out[(i * params.primal.blockSize()) + this->e[i]] ^= 1;
  }
  return out[{0, params.size}];
}

}
