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
   this->params.dual.t * ((size_t) ceil(log2(this->params.dual.blockSize())) + 1)
   + 2 * this->params.primal.t * ((size_t) ceil(log2(this->params.primal.blockSize())))
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
  return std::make_pair(a[{0, params.size}], b[{0, params.size}]);
}

////////////////////////////////////////////////////////////////////////////////
// PCG PROTOCOL METHODS
////////////////////////////////////////////////////////////////////////////////

void PCG::prepare() {
  Timer timer;

  // initialize the pprfs that we are sending
  timer.start("[prepare] sample eXs pprf");
  this->send_eXs = PPRF::sample(
    params.dual.t, LAMBDA, params.primal.k, params.dual.blockSize()
  );
  timer.stop();

  timer.start("[prepare] sample dpfs");
  this->send_eXas_eoe = DPF::sample(params.primal.t, LAMBDA, params.primal.blockSize());
  this->send_eXas = DPF::sample(params.primal.t, LAMBDA, params.primal.blockSize());
  timer.stop();

  // sample public matrices
  timer.start("[prepare] sample primal matrix");
  this->A = LPN::PrimalMatrix(params.pkey, params.primal);
  timer.stop();
  timer.start("[prepare] sample dual matrix");
  this->H = LPN::DualMatrix(params.dkey, params.dual);
  timer.stop();

  // sample primal error vectors
  timer.start("[prepare] sample primal error");
  this->e0 = sampleVector(params.primal.t, params.primal.blockSize());
  this->e1 = sampleVector(params.primal.t, params.primal.blockSize());
  timer.stop();

  // sample dual error vector
  timer.start("[prepare] sample dual error");
  this->epsilon = sampleVector(params.dual.t, params.dual.blockSize());
  timer.stop();

  // sample one primal secret vector
  timer.start("[prepare] sample secret vector");
  this->s0 = BitString::sample(params.primal.k);
  timer.stop();

  // compute the other based on the dual matrix H and error ε
  timer.start("[prepare] compute other secret vector");
  this->s1 = BitString(params.primal.k);
  for (size_t i = 0; i < params.primal.k; i++) {
    for (size_t j = 0 ; j < params.dual.t; j++) {
      if (this->H[{i, j * params.dual.blockSize() + this->epsilon[j]}]) {
        this->s1[i] ^= 1;
      }
    }
  }
  timer.stop();

  // free up this space as it is needed later
  this->H = LPN::DualMatrix();

  // encrypt secret vectors
  timer.start("[prepare] encrypt secret vectors");
  this->enc_s0 = this->ahe.encrypt(this->s0);
  this->enc_s1 = this->ahe.encrypt(this->s1);
  timer.stop();

  // masks for (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) terms
  timer.start("[prepare] sample masks");
  this->send_masks = BitString::sample(this->params.primal.t);
  this->recv_masks = BitString::sample(this->params.primal.t);
  timer.stop();
}

void PCG::online(
  uint32_t other_id, Channel channel, RandomOTSender srots, RandomOTReceiver rrots
) {
  Timer timer;

  float upload = channel->bytesIn;
  float download = channel->bytesOut;

  //////// (⟨aᵢ,s₁⟩ · e₀) and (⟨aᵢ,s₀⟩ · e₁) ⊕ (e₀ ○ e₁) TERMS ////////
  // equality test for (e₀ ○ e₁) terms (in both directions)
  timer.start("[online] equality testing");
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

  timer.stop();
  std::cout << "         upload   = " << channel->bytesIn - upload << " B" << std::endl;
  std::cout << "         download = " << channel->bytesOut - download << " B" << std::endl;
  std::cout << "         total    = " << (channel->bytesIn + channel->bytesOut) - (upload + download) << " B" << std::endl;
  upload = channel->bytesIn;
  download = channel->bytesOut;

  // exchange encrypted secret vectors Enc(s₀) and Enc(s₁)
  timer.start("[online] send ciphertexts");
  std::vector<AHE::Ciphertext> other_enc_s0, other_enc_s1;
  ORDER_BY_ROLE({
    this->ahe.send(this->enc_s0, channel, true);
    this->ahe.send(this->enc_s1, channel, true);
    this->enc_s0.clear();
    this->enc_s1.clear();
  }, {
    other_enc_s0 = this->ahe.receive(this->params.primal.k, channel, true);
    other_enc_s1 = this->ahe.receive(this->params.primal.k, channel, true);
  });

  timer.stop();
  std::cout << "         upload   = " << channel->bytesIn - upload << " B" << std::endl;
  std::cout << "         download = " << channel->bytesOut - download << " B" << std::endl;
  std::cout << "         total    = " << (channel->bytesIn + channel->bytesOut) - (upload + download) << " B" << std::endl;
  upload = channel->bytesIn;
  download = channel->bytesOut;

  // homomorphically compute Enc(⟨aᵢ,s⟩) for both directions
  timer.start("[online] inner product");
  std::vector<AHE::Ciphertext> enc_recv_eXas = homomorphicInnerProduct(other_enc_s1, true);
  std::vector<AHE::Ciphertext> enc_send_eXas = homomorphicInnerProduct(other_enc_s0, false);
  other_enc_s0.clear();
  other_enc_s1.clear();
  timer.stop();

  // swap Enc(⟨aᵢ,s₀⟩) and Enc(⟨aᵢ,s₁⟩)
  timer.start("[online] send ciphertexts back");
  std::vector<AHE::Ciphertext> send_resp, recv_resp;
  ORDER_BY_ROLE({
    this->ahe.send(enc_send_eXas, channel);
    this->ahe.send(enc_recv_eXas, channel);
  }, {
    send_resp = this->ahe.receive(this->params.primal.t, channel);
    recv_resp = this->ahe.receive(this->params.primal.t, channel);
  });

  timer.stop();
  std::cout << "         upload   = " << channel->bytesIn - upload << " B" << std::endl;
  std::cout << "         download = " << channel->bytesOut - download << " B" << std::endl;
  std::cout << "         total    = " << (channel->bytesIn + channel->bytesOut) - (upload + download) << " B" << std::endl;
  upload = channel->bytesIn;
  download = channel->bytesOut;

  BitString send_decrypted = this->ahe.decrypt(send_resp);
  BitString recv_decrypted = this->ahe.decrypt(recv_resp);
  send_resp.clear();
  recv_resp.clear();

  // exchange dpfs for both terms
  timer.start("[online] share (⟨aᵢ,s⟩ · e) pprfs");
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

  timer.stop();
  std::cout << "         upload   = " << channel->bytesIn - upload << " B" << std::endl;
  std::cout << "         download = " << channel->bytesOut - download << " B" << std::endl;
  std::cout << "         total    = " << (channel->bytesIn + channel->bytesOut) - (upload + download) << " B" << std::endl;
  upload = channel->bytesIn;
  download = channel->bytesOut;

  // free up some memory
  this->send_eoe.clear();
  recv_decrypted.clear();
  send_decrypted.clear();

  //////// ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ TERM //////////////////////

  timer.start("[online] send ⟨ε ⊗ s⟩ pprf");
  ORDER_BY_ROLE({
    PPRF::send(this->send_eXs, this->s0, channel, srots);
  }, {
    this->recv_eXs = PPRF::receive(
      this->epsilon, LAMBDA, params.primal.k, params.dual.blockSize(), channel, rrots
    );
  });
  timer.stop();

  std::cout << "         upload   = " << channel->bytesIn - upload << " B" << std::endl;
  std::cout << "         download = " << channel->bytesOut - download << " B" << std::endl;
  std::cout << "         total    = " << (channel->bytesIn + channel->bytesOut) - (upload + download) << " B" << std::endl;
  upload = channel->bytesIn;
  download = channel->bytesOut;
}

std::pair<BitString, BitString> PCG::finalize(size_t other_id) {
  Timer timer;

  // expand out ⟨ε ⊗ s⟩ pprfs
  timer.start("[finalize] expand ⟨ε ⊗ s⟩ pprfs");
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->recv_eXs[i].expand();
    }
  }, this->recv_eXs.size());
  timer.stop();

  // expand only the other pprfs that are needed
  timer.start("[finalize] expand (⟨aᵢ,s⟩ · e) pprfs");
  MULTI_TASK([this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      this->recv_eXas_eoe[i].expand();
      this->recv_eXas[i].expand();
    }
  }, params.blocks());
  timer.stop();

  // in each direction, concatenate the image for each error block to get final output
  timer.start("[finalize] concat images");
  BitString send_out, recv_out;
  for (size_t i = 0; i < params.blocks(); i++) {
    BitString send_image = this->send_eXas_eoe[i].image() ^ this->recv_eXas[i].image();
    BitString recv_image = this->recv_eXas_eoe[i].image() ^ this->send_eXas[i].image();

    // at our error position, xor with our shares of (e₀ ○ e₁)
    send_image[this->e0[i]] ^= this->send_masks[i];
    recv_image[this->e1[i]] ^= this->recv_masks[i] ^ recv_eoe[i];

    send_out += send_image;
    recv_out += recv_image;
  }
  timer.stop();

  // free up used memory
  timer.start("[finalize] clear memory");
  for (DPF& dpf : this->send_eXas_eoe) { dpf.clear(); }
  for (DPF& dpf : this->recv_eXas_eoe) { dpf.clear(); }
  for (DPF& dpf : this->send_eXas)     { dpf.clear(); }
  for (DPF& dpf : this->recv_eXas)     { dpf.clear(); }
  timer.stop();

  // recompute dual matrix needed for next step
  timer.start("[finalize] resample dual matrix");
  this->H = LPN::DualMatrix(params.dkey, params.dual);
  this->B = LPN::MatrixProduct(A, H);
  timer.stop();

  // compute shares of the ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ vector
  timer.start("[finalize] compute last term");

  auto baex_shares = TASK_REDUCE<std::pair<BitString, BitString>>(
    [this](size_t start, size_t end)
  {
    BitString send_out(end - start), recv_out(end - start);
    for (size_t i = start; i < end; i++) {
      BitString send_aXeXs(params.dual.N()), recv_aXeXs(params.dual.N());
      for (uint32_t idx : this->A.getNonZeroElements(i)) {
        for (size_t n = 0; n < params.dual.N(); n++) {
          send_aXeXs[n] ^= this->send_eXs[n / params.dual.blockSize()].getImage()->operator[](n % params.dual.blockSize())[idx];
          recv_aXeXs[n] ^= this->recv_eXs[n / params.dual.blockSize()].getImage()->operator[](n % params.dual.blockSize())[idx];
        }
      }
      BitString row = this->B[i];
      send_out[i - start] = row * send_aXeXs;
      recv_out[i - start] = row * recv_aXeXs;
    }
    return std::make_pair(send_out, recv_out);
  }, [](std::vector<std::pair<BitString, BitString>> pairs) {
    BitString first, second;
    for (std::pair<BitString, BitString> pair : pairs) {
      first += pair.first;
      second += pair.second;
    }
    return std::make_pair(first, second);
  }, params.size);
  timer.stop();

  send_out ^= baex_shares.first;
  recv_out ^= baex_shares.second;

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

std::vector<AHE::Ciphertext> PCG::homomorphicInnerProduct(
  const std::vector<AHE::Ciphertext>& enc_s, bool sender
) const {
  std::vector<AHE::Ciphertext> recv_aXs;
  for (size_t i = 0; i < params.primal.t; i++) {
    uint32_t idx = (i * this->params.primal.blockSize()) + (sender ? this->e0[i] : this->e1[i]);

    // homomorphically compute the inner product of A[idx] and enc(s1)
    std::vector<uint32_t> points = this->A.getNonZeroElements(idx);
    AHE::Ciphertext ctx = enc_s[points[0]];
    for (size_t i = 1; i < points.size(); i++) {
      ctx = this->ahe.add(ctx, enc_s[points[i]]);
    }

    // add our share to get their share
    ctx = this->ahe.add(ctx, (sender ? this->send_masks[i] : this->recv_masks[i]));

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
