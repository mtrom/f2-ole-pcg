#include "pkg/pcg.hpp"

#include <cmath>
#include <iostream>
#include <thread>

#include "ahe/ahe.hpp"
#include "pkg/eqtest.hpp"
#include "pkg/pprf.hpp"
#include "util/concurrency.hpp"
#include "util/defines.hpp"
#include "util/timer.hpp"

#define CHOOSE_BY_ROLE(block1, block2) \
  if (this->id < other_id) {           \
    block1;                            \
  } else {                             \
    block2;                            \
  }

#define ORDER_BY_ROLE(block1, block2) \
  if (this->id < other_id) {          \
    block1;                           \
    block2;                           \
  } else {                            \
    block2;                           \
    block1;                           \
  }

namespace Beaver {

std::pair<size_t, size_t> PCG::numOTs(uint32_t other_id) const {
  size_t pprfs = (
   this->params.dual.t * ((size_t) ceil(log2(this->params.dual.N())) + 1)
   + 2 * this->params.primal.t * ((size_t) ceil(log2(this->params.primal.blockSize())) + 1)
  );
  size_t eqtests = EqTest::numOTs(
    this->params.primal.errorBits(), this->params.eqTestThreshold, 2 * this->params.primal.t
  );

  // pprfs are symmetric but eqtests are not (i.e., one party is the sender / one is receiver)
  CHOOSE_BY_ROLE({
    return std::make_pair(pprfs + eqtests, pprfs);
  }, {
    return std::make_pair(pprfs, pprfs + eqtests);
  });
}

// the programmed inputs are just the output of the lpn instances
std::pair<BitString, BitString> PCG::inputs() const {
  BitString a = this->A * this->s0;
  BitString b = this->A * this->s1;
  for (size_t i = 0; i < params.primal.t; i++) {
    a[(i * params.primal.blockSize()) + this->e0[i]] ^= 1;
    b[(i * params.primal.blockSize()) + this->e1[i]] ^= 1;
  }
  return std::make_pair(a, b);
}

////////////////////////////////////////////////////////////////////////////////
// PCG PROTOCOL METHODS
////////////////////////////////////////////////////////////////////////////////

PCG::PCG(uint32_t id, const PCGParams& params)
  : id(id), params(params), ahe(params.primal.k), A(params.pkey, params.primal),
    H(params.dkey, params.dual)
{
  this->B = this->A * this->H;

  // sample primal error vectors
  this->e0 = sampleVector(params.primal.t, params.primal.blockSize());
  this->e1 = sampleVector(params.primal.t, params.primal.blockSize());

  // sample dual error vector
  this->epsilon = sampleDistinct(params.dual.t, params.dual.N());

  // sample one primal secret vector
  this->s0 = BitString::sample(params.primal.k);

  // compute the other based on the dual matrix H and error ε
  this->s1 = BitString(params.primal.k);
  for (size_t i = 0; i < params.primal.k; i++) {
    for (uint32_t error : this->epsilon) {
      if (H[{i, error}]) { this->s1[i] ^= 1; }
    }
  }
}

void PCG::prepare() {
  // encrypt secret vectors
  this->enc_s0 = this->ahe.encrypt(this->s0);
  this->enc_s1 = this->ahe.encrypt(this->s1);

  // masks for (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) terms
  this->send_masks = BitString::sample(this->params.primal.t);
  this->recv_masks = BitString::sample(this->params.primal.t);

  // initialize the pprfs that we are sending
  this->send_eXs = PPRF::sample(params.dual.t, LAMBDA, params.primal.k, params.dual.N());
  this->send_eXas_eoe = DPF::sample(params.primal.t, LAMBDA, params.primal.blockSize());
  this->send_eXas = DPF::sample(params.primal.t, LAMBDA, params.primal.blockSize());
}

void PCG::online(
  uint32_t other_id, Channel channel, RandomOTSender srots, RandomOTReceiver rrots
) {

  //////// (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) TERMS ////////

  // equality test for (e₀ ○ e₁) terms (in both directions)
  CHOOSE_BY_ROLE({
    EqTestSender eqtester = EqTestSender(
      params.primal.errorBits(), params.eqTestThreshold, 2 * params.primal.t, channel, srots
    );
    std::tie(this->recv_eoe, this->send_eoe) = eqtester.run(this->e1, this->e0);
  }, {
    EqTestReceiver eqtester = EqTestReceiver(
      params.primal.errorBits(), params.eqTestThreshold, 2 * params.primal.t, channel, rrots
    );
    std::tie(this->send_eoe, this->recv_eoe) = eqtester.run(this->e0, this->e1);
  });

  // exchange encrypted secret vectors Enc(s₀) and Enc(s₁)
  std::vector<AHE::Ciphertext> other_enc_s0, other_enc_s1;
  ORDER_BY_ROLE({
    this->ahe.send(this->enc_s0, channel);
    this->ahe.send(this->enc_s1, channel);
  }, {
    other_enc_s0 = this->ahe.receive(this->params.primal.k, channel);
    other_enc_s1 = this->ahe.receive(this->params.primal.k, channel);
  });

  // homomorphically compute Enc(⟨aᵢ,s⟩) for both directions
  std::vector<AHE::Ciphertext> enc_recv_eXas = homomorphicInnerProduct(
    ahe, other_enc_s1, this->send_masks, this->e0
  );
  std::vector<AHE::Ciphertext> enc_send_eXas = homomorphicInnerProduct(
    ahe, other_enc_s0, this->recv_masks, this->e1
  );

  // swap Enc(⟨aᵢ,s₀⟩) and Enc(⟨aᵢ,s₁⟩)
  std::vector<AHE::Ciphertext> send_resp, recv_resp;
  ORDER_BY_ROLE({
    this->ahe.send(enc_send_eXas, channel);
    this->ahe.send(enc_recv_eXas, channel);
  }, {
    send_resp = this->ahe.receive(this->params.primal.t, channel);
    recv_resp = this->ahe.receive(this->params.primal.t, channel);
  });

  BitString send_decrypted = this->ahe.decrypt(send_resp);
  BitString recv_decrypted = this->ahe.decrypt(recv_resp);

  // exchange dpfs for both terms
  ORDER_BY_ROLE({
    DPF::send(this->send_eXas_eoe, send_decrypted ^ send_eoe, channel, srots);
    this->recv_eXas = DPF::receive(
      this->e0, LAMBDA, params.primal.blockSize(), channel, rrots
    );
  }, {
    this->recv_eXas_eoe = DPF::receive(
      this->e1, LAMBDA, params.primal.blockSize(), channel, rrots
    );
    DPF::send(this->send_eXas, recv_decrypted, channel, srots);
  });

  //////// ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ TERM //////////////////////

  ORDER_BY_ROLE({
    PPRF::send(this->send_eXs, this->s0, channel, srots);
  }, {
    this->recv_eXs = PPRF::receive(
      this->epsilon, LAMBDA, params.primal.k, params.dual.N(), channel, rrots
    );
  });
}

std::pair<BitString, BitString> PCG::finalize(size_t other_id) {
  // in each direction, concatenate the image for each error block to get final output
  BitString send_out, recv_out;
  for (size_t i = 0; i < params.primal.t; i++) {
    BitString send_image = this->send_eXas_eoe[i].image() ^ this->recv_eXas[i].image();
    BitString recv_image = this->recv_eXas_eoe[i].image() ^ this->send_eXas[i].image();

    // at our error position, xor with our shares of (e₀ ○ e₁)
    send_image[this->e0[i]] ^= this->send_masks[i];
    recv_image[this->e1[i]] ^= this->recv_masks[i] ^ recv_eoe[i];

    send_out += send_image;
    recv_out += recv_image;
  }
  send_out ^= secretTensorProcessing(this->send_eXs);
  recv_out ^= secretTensorProcessing(this->recv_eXs);

  return (
    (id < other_id) ? std::make_pair(send_out, recv_out) : std::make_pair(recv_out, send_out)
  );
}

std::pair<BitString, BitString> PCG::run(
  uint32_t other_id, Channel channel, RandomOTSender srots, RandomOTReceiver rrots
) {
  prepare();
  online(other_id, channel, srots, rrots);
  return finalize(other_id);
};

////////////////////////////////////////////////////////////////////////////////
// HELPER METHODS
////////////////////////////////////////////////////////////////////////////////

BitString PCG::secretTensorProcessing(std::vector<PPRF> pprfs) const {
  // arrange our shares of the ε ⊗ s matrix by column
  std::vector<BitString> eXs(params.primal.k, BitString(params.dual.N()));
  MULTI_TASK([this, &eXs, &pprfs](size_t start, size_t end) {
    for (size_t c = start; c < end; c++) {
      for (size_t r = 0; r < params.dual.N(); r++) {
        for (size_t p = 0; p < params.dual.t; p++) {
          eXs[c][r] ^= pprfs[p](r)[c];
        }
      }
    }
  }, params.primal.k);

  // then use those to compute shares of the ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ vector
  return TASK_REDUCE<BitString>([this, &eXs](size_t start, size_t end) {
    BitString out(end - start);
    for (size_t i = start; i < end; i++) {
      BitString aXeXs;
      for (uint32_t idx : this->A.getNonZeroElements(i)) {
        if (aXeXs.size() == 0) { aXeXs = eXs[idx];  }
        else                   { aXeXs ^= eXs[idx]; }
      }
      out[i - start] = this->B[i] * aXeXs;
    }
    return out;
  }, BitString::concat, params.size);
}

// TODO: ahe / error really don't need to be passed in
std::vector<AHE::Ciphertext> PCG::homomorphicInnerProduct(
  const AHE& ahe,
  const std::vector<AHE::Ciphertext>& enc_s,
  const BitString& masks,
  const std::vector<uint32_t>& error
) const {
  std::vector<AHE::Ciphertext> recv_aXs;
  for (size_t i = 0; i < error.size(); i++) {
    uint32_t idx = (i * this->params.primal.blockSize()) + error[i];

    // homomorphically compute the inner product of A[idx] and enc(s1)
    std::set<uint32_t> pointset = this->A.getNonZeroElements(idx);
    std::vector<uint32_t> points(pointset.begin(), pointset.end()); // TODO: don't need this?
    AHE::Ciphertext ctx = enc_s[points[0]];
    for (size_t i = 1; i < points.size(); i++) {
      ctx = ahe.add(ctx, enc_s[points[i]]);
    }

    // add our share to get their share
    ctx = ahe.add(ctx, masks[i]);

    recv_aXs.push_back(ctx);
  }
  return recv_aXs;
}

////////////////////////////////////////////////////////////////////////////////
// TRIPLES OBJECT
////////////////////////////////////////////////////////////////////////////////

Triple Triples::get() {
  if (this->remaining() == 0) { throw std::out_of_range("[Triples::get] out of triples"); }
  Triple out(this->a[this->used], this->b[this->used], this->c[this->used]);
  this->used++;
  return out;
}

Triples Triples::reserve(size_t n) {
  if (this->remaining() < n) { throw std::out_of_range("[Triples::reserve] out of triples"); }
  Triples out(
    this->a[{used, used + n}],this->b[{used, used + n}], this->c[{used, used + n}]
  );
  used += n;
  return out;
}

Triples transform(
  BitString a,
  BitString b,
  std::vector<BitString> cij,
  std::vector<BitString> dji,
  std::vector<BitString> seeds
) {
  if (a.size() != b.size()) {
    throw::invalid_argument("[Beaver::transform] mismatched BitString sizes");
  } else if (cij.size() != dji.size() || dji.size() != seeds.size()) {
    throw::invalid_argument("[Beaver::transform] mismatched vector sizes");
  }

  BitString ab = a & b;
  for (size_t i = 0; i < cij.size(); i++) {
    ab ^= cij[i] ^ dji[i] ^ seeds[i].aes(ab.size());
  }

  return Triples(a, b, ab);
}


////////////////////////////////////////////////////////////////////////////////
// MOCK METHODS
////////////////////////////////////////////////////////////////////////////////

MockPCG::MockPCG(uint32_t id, size_t size)
  : id(id), sender(size, BitString::fromUInt(id) + BitString::fromUInt(0x00)),
    receiver(size, BitString::fromUInt(id) + BitString::fromUInt(0x01)) { }

std::tuple<BitString, BitString, BitString> MockPCG::run(uint32_t id) {
  BitString skey = BitString::fromUInt(id) + BitString::fromUInt(0x00);
  BitString rkey = BitString::fromUInt(id) + BitString::fromUInt(0x01);

  if (this->id < id) {
    BitString seed = BitString::fromUInt(this->id) + BitString::fromUInt(id);
    return std::make_tuple(
      seed, sender.run(rkey), receiver.run(skey)
    );
  } else {
    BitString seed = BitString::fromUInt(id) + BitString::fromUInt(this->id);
    return std::make_tuple(
      seed, receiver.run(skey), sender.run(rkey)
    );
  }
}

std::pair<BitString, BitString> MockPCG::inputs() const {
  return std::make_pair(sender.lpnOutput(), receiver.lpnOutput());
}

BitString MockSender::run(BitString key) const {
  PRF<BitString> their_prf(key);

  BitString a = this->prf(0x0A, this->size);
  BitString b = their_prf(0x0B, this->size);
  BitString c = a & b;
  return c ^ this->prf(0x0C, this->size);
}

BitString MockReceiver::run(BitString key) const {
  PRF<BitString> their_prf(key);
  return their_prf(0x0C, this->size);
}

BitString MockSender::lpnOutput() const {
  return this->prf(0x0A, this->size);
}

BitString MockReceiver::lpnOutput() const {
  return this->prf(0x0B, this->size);
}

}
