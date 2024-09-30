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
#include "util/transpose.hpp"

#define REPORT_COMMS() \
  std::cout << "         upload   = " << channel->bytesIn - upload << " B" << std::endl;    \
  std::cout << "         download = " << channel->bytesOut - download << " B" << std::endl; \
  std::cout << "         total    = ";                                                      \
  std::cout << (channel->bytesIn + channel->bytesOut) - (upload + download);                \
  std::cout << " B" << std::endl;                                                           \
  upload = channel->bytesIn;                                                                \
  download = channel->bytesOut;


namespace Beaver {

////////////////////////////////////////////////////////////////////////////////
// PCG PROTOCOL METHODS
////////////////////////////////////////////////////////////////////////////////

void PCG::init() {
  Timer timer("[prepare] sample public matrices");
  this->A = LPN::PrimalMatrix(params.pkey, params.primal);
  this->H = LPN::DualMatrix(params.dkey, params.dual);
  this->B = LPN::MatrixProduct(A, H);
  timer.stop();
}

void Sender::prepare() {
  Timer timer;

  // initialize the pprfs that we are sending
  timer.start("[prepare] sample (ε ⊗ s) pprfs");
  this->eXs = PPRF::sample(
    params.dual.t, LAMBDA, params.primal.k, params.dual.blockSize()
  );
  timer.stop();

  timer.start("[prepare] sample (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁)  pprfs");
  this->eXas_eoe = DPF::sample(params.primal.t, LAMBDA, params.primal.blockSize());
  timer.stop();

  // sample primal error vectors
  timer.start("[prepare] sample primal error");
  this->e = sampleVector(params.primal.t, params.primal.blockSize());
  timer.stop();

  // sample primal secret vector
  timer.start("[prepare] sample secret vector");
  this->s = BitString::sample(params.primal.k);
  timer.stop();

  // encrypt secret vectors
  timer.start("[prepare] encrypt secret vectors");
  this->enc_s = this->ahe.encrypt(this->s);
  timer.stop();

  // masks for (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) terms
  timer.start("[prepare] sample masks");
  this->masks = BitString::sample(this->params.primal.t);
  timer.stop();
}

void Receiver::prepare() {
  Timer timer;

  timer.start("[prepare] sample (⟨aᵢ,s₀⟩ · e₁) pprfs");
  this->eXas = DPF::sample(params.primal.t, LAMBDA, params.primal.blockSize());
  timer.stop();

  // sample primal error vectors
  timer.start("[prepare] sample primal error");
  this->e = sampleVector(params.primal.t, params.primal.blockSize());
  timer.stop();

  // sample dual error vector
  timer.start("[prepare] sample dual error");
  this->epsilon = sampleVector(params.dual.t, params.dual.blockSize());
  timer.stop();

  // compute secret vector based on the dual matrix H and error ε
  timer.start("[prepare] compute other secret vector");
  this->s = BitString(params.primal.k);
  for (size_t i = 0; i < params.primal.k; i++) {
    for (size_t j = 0 ; j < params.dual.t; j++) {
      if (this->H[{i, j * params.dual.blockSize() + this->epsilon[j]}]) {
        this->s[i] ^= 1;
      }
    }
  }
  timer.stop();

  // encrypt secret vectors
  timer.start("[prepare] encrypt secret vectors");
  this->enc_s = this->ahe.encrypt(this->s);
  timer.stop();

  // masks for (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) terms
  timer.start("[prepare] sample masks");
  this->masks = BitString::sample(this->params.primal.t);
  timer.stop();
}

void Sender::online(Channel channel, RandomOTSender srots, RandomOTReceiver rrots) {
  Timer timer;

  float upload = channel->bytesIn;
  float download = channel->bytesOut;

  // equality test for (e₀ ○ e₁) terms
  timer.start("[online] equality testing");
  BitString eoe = EqTestSender(
    params.primal.errorBits(), params.eqTestThreshold, params.primal.t, channel, srots
  ).run(this->e);
  timer.stop();
  REPORT_COMMS();

  // exchange encrypted secret vectors Enc(s₀) and Enc(s₁)
  timer.start("[online] send ciphertexts");
  this->ahe.send(this->enc_s, channel, true);
  this->enc_s.clear();
  std::vector<AHE::Ciphertext> other_enc_s = this->ahe.receive(
    this->params.primal.k, channel, true
  );
  timer.stop();
  REPORT_COMMS();

  // homomorphically compute Enc(⟨aᵢ,s⟩)
  timer.start("[online] inner product");
  std::vector<AHE::Ciphertext> enc_eXas = homomorphicInnerProduct(other_enc_s);
  other_enc_s.clear();
  timer.stop();

  // swap Enc(⟨aᵢ,s₀⟩) and Enc(⟨aᵢ,s₁⟩)
  timer.start("[online] send ciphertexts back");
  this->ahe.send(enc_eXas, channel);
  std::vector<AHE::Ciphertext> resp = this->ahe.receive(this->params.primal.t, channel);
  timer.stop();
  REPORT_COMMS();

  timer.start("[online] decrypt response");
  BitString decrypted_resp = this->ahe.decrypt(resp);
  resp.clear();
  timer.stop();

  // exchange dpfs for both terms
  timer.start("[online] exchange (⟨aᵢ,s⟩ · e) pprfs");
  DPF::send(this->eXas_eoe, decrypted_resp ^ eoe, channel, srots);
  this->eXas = DPF::receive(
    this->e, LAMBDA, params.primal.blockSize(), channel, rrots
  );
  decrypted_resp.clear();
  timer.stop();
  REPORT_COMMS();

  timer.start("[online] send (ε ⊗ s) pprf");
  PPRF::send(this->eXs, this->s, channel, srots);
  timer.stop();
  REPORT_COMMS();
}

void Receiver::online(Channel channel, RandomOTSender srots, RandomOTReceiver rrots) {
  Timer timer;

  float upload = channel->bytesIn;
  float download = channel->bytesOut;

  // equality test for (e₀ ○ e₁) terms
  timer.start("[online] equality testing");
  this->eoe = EqTestReceiver(
    params.primal.errorBits(), params.eqTestThreshold, params.primal.t, channel, rrots
  ).run(this->e);
  timer.stop();
  REPORT_COMMS();

  // exchange encrypted secret vectors Enc(s₀) and Enc(s₁)
  timer.start("[online] send ciphertexts");
  std::vector<AHE::Ciphertext> other_enc_s = this->ahe.receive(
    this->params.primal.k, channel, true
  );
  this->ahe.send(this->enc_s, channel, true);
  this->enc_s.clear();
  timer.stop();
  REPORT_COMMS();

  // homomorphically compute Enc(⟨aᵢ,s⟩)
  timer.start("[online] inner product");
  std::vector<AHE::Ciphertext> enc_eXas = homomorphicInnerProduct(other_enc_s);
  other_enc_s.clear();
  timer.stop();

  // swap Enc(⟨aᵢ,s₀⟩) and Enc(⟨aᵢ,s₁⟩)
  timer.start("[online] send ciphertexts back");
  std::vector<AHE::Ciphertext> resp = this->ahe.receive(this->params.primal.t, channel);
  this->ahe.send(enc_eXas, channel);
  timer.stop();
  REPORT_COMMS();

  timer.start("[online] decrypt response");
  BitString decrypted_resp = this->ahe.decrypt(resp);
  resp.clear();
  timer.stop();

  // exchange dpfs for both terms
  timer.start("[online] exchange (⟨aᵢ,s⟩ · e) pprfs");
  this->eXas_eoe = DPF::receive(
    this->e, LAMBDA, params.primal.blockSize(), channel, rrots
  );
  DPF::send(this->eXas, decrypted_resp, channel, srots);
  decrypted_resp.clear();
  timer.stop();
  REPORT_COMMS();

  timer.start("[online] receive (ε ⊗ s) pprf");
  this->eXs = PPRF::receive(
    this->epsilon, LAMBDA, params.primal.k, params.dual.blockSize(), channel, rrots
  );
  timer.stop();
  REPORT_COMMS();
}

void Sender::finalize() {
  Timer timer;

  // arrange our shares of the ε ⊗ s matrix by column
  timer.start("[finalize] transpose (ε ⊗ s) matrix");
  this->eXs_matrix = transpose(this->eXs, params);
  timer.stop();

  // expand only the other pprfs that are needed
  timer.start("[finalize] expand (⟨aᵢ,s⟩ · e) pprfs");
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->eXas[i].expand();
    }
  }, params.primal.t);
  timer.stop();

  // concatenate the image for each error block to get final output
  timer.start("[finalize] concat images");
  for (size_t i = 0; i < params.primal.t; i++) {
    BitString image = this->eXas_eoe[i].image() ^ this->eXas[i].image();

    // at our error position, xor with our shares of (e₀ ○ e₁)
    image[this->e[i]] ^= this->masks[i];

    this->output += image;
  }
  if (this->output.size() != params.size) { this->output.resize(params.size); }
  timer.stop();

  // free up used memory
  timer.start("[finalize] clear memory");
  for (DPF& dpf : this->eXas_eoe) { dpf.clear(); }
  for (DPF& dpf : this->eXas)     { dpf.clear(); }
  timer.stop();
}

void Receiver::finalize() {
  Timer timer;

  timer.start("[finalize] expand (ε ⊗ s) pprfs");
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->eXs[i].expand();
    }
  }, this->eXs.size());
  timer.stop();

  // arrange our shares of the ε ⊗ s matrix by column
  timer.start("[finalize] transpose (ε ⊗ s) matrix");
  this->eXs_matrix = transpose(this->eXs, params);
  timer.stop();

  timer.start("[finalize] expand (⟨aᵢ,s⟩ · e) ⊕ (e₀ ○ e₁) pprfs");
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->eXas_eoe[i].expand();
    }
  }, params.primal.t);
  timer.stop();

  // in each direction, concatenate the image for each error block to get final output
  timer.start("[finalize] concat images");
  for (size_t i = 0; i < params.blocks(); i++) {
    BitString image = this->eXas_eoe[i].image() ^ this->eXas[i].image();

    // at our error position, xor with our shares of (e₀ ○ e₁)
    image[this->e[i]] ^= this->masks[i] ^ this->eoe[i];

    this->output += image;
  }
  if (this->output.size() != params.size) { this->output.resize(params.size); }
  timer.stop();

  timer.start("[finalize] clear memory");
  for (DPF& dpf : this->eXas_eoe) { dpf.clear(); }
  for (DPF& dpf : this->eXas)     { dpf.clear(); }
  timer.stop();
}

void PCG::expand() {
  // compute shares of the ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ vector
  Timer timer("[finalize] compute last term");
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
  timer.stop();
}


std::vector<AHE::Ciphertext> PCG::homomorphicInnerProduct(
  const std::vector<AHE::Ciphertext>& enc_s
) const {
  std::vector<AHE::Ciphertext> out;
  for (size_t i = 0; i < params.primal.t; i++) {
    uint32_t idx = (i * this->params.primal.blockSize()) + this->e[i];

    // homomorphically compute the inner product of A[idx] and enc(s1)
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
BitString PCG::inputs() const {
  BitString out = this->A * this->s;
  for (size_t i = 0; i < params.primal.t; i++) {
    out[(i * params.primal.blockSize()) + this->e[i]] ^= 1;
  }
  return out[{0, params.size}];
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
