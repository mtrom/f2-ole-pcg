#include "pkg/pcg.hpp"

#include <cmath>
#include <iostream>

#include "pkg/eqtest.hpp"
#include "pkg/pprf.hpp"
#include "ahe/ahe.hpp"
#include "util/defines.hpp"

namespace Beaver {

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
// PCG RUN
////////////////////////////////////////////////////////////////////////////////

std::pair<BitString, BitString> PCG::run(
  uint32_t id, std::shared_ptr<CommParty> channel, RandomOTSender srots, RandomOTReceiver rrots
) const {
  if (this->id < id) {
    return std::make_pair(
      sender.run(channel, srots, rrots), receiver.run(channel, srots, rrots)
    );
  } else {
    return std::make_pair(
      receiver.run(channel, srots, rrots), sender.run(channel, srots, rrots)
    );
  }
}

std::pair<BitString, BitString> PCG::inputs() const {
  return std::make_pair(sender.lpnOutput(), receiver.lpnOutput());
}

////////////////////////////////////////////////////////////////////////////////
// SENDER / RECEIVER CONSTRUCTORS
////////////////////////////////////////////////////////////////////////////////

Base::Base(const PCGParams& params)
  : params(params), A(params.pkey, params.primal), H(params.dkey, params.dual)
{
  this->B = this->A * this->H;
  this->e = sampleVector(params.primal.t, params.primal.blockSize());
}

Sender::Sender(const PCGParams& params) : Base(params) {
  // sample secret vector & regular errors
  this->s = BitString::sample(params.primal.k);
}

Receiver::Receiver(const PCGParams& params) : Base(params) {
  // sample regular primal errors & dual errors
  this->epsilon = sampleDistinct(params.dual.t, params.dual.N());

  // set our s by computing Hε
  this->s = BitString(params.primal.k);
  for (size_t i = 0; i < params.primal.k; i++) {
    for (uint32_t error : this->epsilon) {
      if (H[{i, error}]) { this->s[i] ^= 1; }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// SENDER / RECEIVER RUN
////////////////////////////////////////////////////////////////////////////////

BitString Sender::run(
  std::shared_ptr<CommParty> channel, RandomOTSender srots, RandomOTReceiver rrots
) const {
  return (
    this->secretTensor(channel, srots)
    ^ this->sendInnerProductTerm(channel, srots)
    ^ this->receiveInnerProductTerm(channel, rrots)
    ^ this->errorProduct(channel, srots)
  );
}

BitString Receiver::run(
  std::shared_ptr<CommParty> channel, RandomOTSender srots, RandomOTReceiver rrots
) const {
  return (
    this->secretTensor(channel, rrots)
    ^ this->receiveInnerProductTerm(channel, rrots)
    ^ this->sendInnerProductTerm(channel, srots)
    ^ this->errorProduct(channel, rrots)
  );
}

BitString Base::lpnOutput() const {
  BitString out = this->A * this->s;
  for (size_t i = 0; i < params.primal.t; i++) {
    out[(i * params.primal.blockSize()) + this->e[i]] ^= 1;
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// SENDER / RECEIVER ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ TERM
////////////////////////////////////////////////////////////////////////////////

BitString Base::secretTensorProcessing(std::vector<PPRF> pprfs) const {
  // arrange the ε ⊗ s matrix by columns
  std::vector<BitString> eXs(params.primal.k, BitString(params.dual.N()));
  for (size_t c = 0; c < params.primal.k; c++) {
    for (size_t r = 0; r < params.dual.N(); r++) {
      for (size_t p = 0; p < params.dual.t; p++) {
        eXs[c][r] ^= pprfs[p](r)[c];
      }
    }
  }

  BitString out(params.size);
  for (size_t i = 0; i < params.size; i++) {
    BitString aXeXs;
    for (uint32_t idx : A.getNonZeroElements(i)) {
      if (aXeXs.size() == 0) { aXeXs = eXs[idx];  }
      else                   { aXeXs ^= eXs[idx]; }
    }

    out[i] = B[i] * aXeXs;
  }

  return out;
}

BitString Sender::secretTensor(
  std::shared_ptr<CommParty> channel,
  RandomOTSender rots
) const {
  std::vector<PPRF> pprfs = PPRF::sample(params.dual.t, LAMBDA, params.primal.k, params.dual.N());
  PPRF::send(pprfs, this->s, channel, rots);
  return secretTensorProcessing(pprfs);
}

BitString Receiver::secretTensor(
  std::shared_ptr<CommParty> channel,
  RandomOTReceiver rots
) const {
  std::vector<PPRF> pprfs = PPRF::receive(
    this->epsilon, LAMBDA, params.primal.k, params.dual.N(), channel, rots
  );
  return secretTensorProcessing(pprfs);
}

////////////////////////////////////////////////////////////////////////////////
// SENDER / RECEIVER ⟨aᵢ,s⟩ · e TERM
////////////////////////////////////////////////////////////////////////////////

BitString Base::sendInnerProductTerm(
  std::shared_ptr<CommParty> channel,
  RandomOTSender rots
) const {
  AHE ahe;
  AHEUtils::send(ahe.encrypt(this->s), channel);
  std::vector<AHE::Ciphertext> response = AHEUtils::receive(this->params.primal.t, channel);
  BitString aXs = ahe.decrypt(response);

  // use inner product share as payload for pprfs
  std::vector<PPRF> pprfs = PPRF::sample(this->e.size(), LAMBDA, 1, params.primal.blockSize());
  PPRF::sendDPFs(pprfs, aXs, channel, rots);

  BitString out;
  for (size_t i = 0; i < this->e.size(); i++) {
    out += pprfs[i].image();
  }
  return out;
}

BitString Base::receiveInnerProductTerm(
  std::shared_ptr<CommParty> channel,
  RandomOTReceiver rots
) const {
  AHE ahe;
  std::vector<AHE::Ciphertext> s = AHEUtils::receive(this->params.primal.k, channel);

  // my share of ⟨aᵢ,s⟩
  BitString share = BitString::sample(this->params.primal.t);

  std::vector<AHE::Ciphertext> aXs;
  for (size_t i = 0; i < this->e.size(); i++) {
    uint32_t idx = (i * this->params.primal.blockSize()) + this->e[i];

    // homomorphically compute the inner product of A[idx] and s
    AHE::Ciphertext ctx;
    for (uint32_t point : A.getNonZeroElements(idx)) {
      if (ctx.size() == 0) { ctx = s[point]; }
      else                 { ctx = ahe.add(ctx, s[point]); }
    }

    // add our share to get their share
    ctx = ahe.add(ctx, share[i]);

    aXs.push_back(ctx);
  }

  AHEUtils::send(aXs, channel);

  // retrieve the pprf for each regular error block
  std::vector<PPRF> pprfs = PPRF::receive(
    this->e, LAMBDA, 1, params.primal.blockSize(), channel, rots
  );

  BitString out;
  for (size_t i = 0; i < this->e.size(); i++) {
    BitString image = pprfs[i].image();

    // at our error position, xor with our share
    image[this->e[i]] ^= share[i];

    out += image;
  }

  return out;
}

////////////////////////////////////////////////////////////////////////////////
// SENDER / RECEIVER (e₀ ○ e₁) TERM
////////////////////////////////////////////////////////////////////////////////

BitString Sender::errorProduct(
  std::shared_ptr<CommParty> channel, RandomOTSender rots
) const {
  // run equality testing to get shares of e0[i] == e1[i]
  EqTestSender eqtest(
    params.primal.errorBits(), params.eqTestThreshold, this->e.size(), channel, rots
  );
  eqtest.init();
  BitString shares = eqtest.run(this->e);

  // TODO: we actually want to combine these with one of the ⟨aᵢ, s ⟩ · e₁
  // terms to only use one pprf

  // create the pprfs for each regular error block & send with shares as payload
  std::vector<PPRF> pprfs = PPRF::sample(this->e.size(), LAMBDA, 1, params.primal.blockSize());
  PPRF::sendDPFs(pprfs, shares, channel, rots);

  BitString out;
  for (size_t i = 0; i < this->e.size(); i++) {
    out += pprfs[i].image();
  }
  return out;
}

BitString Receiver::errorProduct(
  std::shared_ptr<CommParty> channel, RandomOTReceiver rots
) const {
  // run equality testing to get shares of e0[i] == e1[i]
  EqTestReceiver eqtest(
    params.primal.errorBits(), params.eqTestThreshold, this->e.size(), channel, rots
  );
  eqtest.init();
  BitString shares = eqtest.run(this->e);

  // retrieve the pprf for each regular error block
  std::vector<PPRF> pprfs = PPRF::receive(
    this->e, (size_t) LAMBDA, (size_t) 1, params.primal.blockSize(), channel, rots
  );

  BitString out;
  for (size_t i = 0; i < this->e.size(); i++) {
    BitString image = pprfs[i].image();

    // at our error position, output the e0[i] == e1[i] share
    image[this->e[i]] ^= shares[i];

    out += image;
  }

  return out;
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
