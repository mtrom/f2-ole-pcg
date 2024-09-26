#pragma once

#include <utility>
#include <tuple>

#include <libscapi/include/comm/Comm.hpp>

#include "pkg/rot.hpp"
#include "util/bitstring.hpp"

// P(unctured) P(seudo)R(andom) F(unction)
class PPRF {
public:
  PPRF() { }

  // initialize given the root `key`
  PPRF(BitString key, size_t outsize, size_t domainsize);

  // initialize the pprf which has been punctured at `points`
  PPRF(
    std::vector<BitString> keys, std::vector<uint32_t> points, size_t outsize, size_t domainsize
  );

  // create pprf with `n` puncture points with randomly sampled keys
  static PPRF sample(size_t n, size_t keysize, size_t outsize, size_t domainsize);

  // create `n` pprfs with a single puncture point with randomly sampled keys
  static std::vector<PPRF> sampleMany(
    size_t n, size_t keysize, size_t outsize, size_t domainsize
  );

  // evaluate the pprf on `x`
  BitString operator() (uint32_t x) const;

  // expand a pprf that has been shared
  void expand();

  // combine two pprfs by xoring their output
  PPRF& operator^=(const PPRF& other);

  size_t domain() const { return domainsize; }

  void clear() { leafs.reset(); levels.clear(); keys.clear(); points.clear(); }

  // for debugging purposes
  std::string toString() const;

  // share across `channel` punctured according to `points` with output `payload`
  static void send(
    PPRF pprf, BitString payload,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
  );
  static PPRF receive(
    std::vector<uint32_t> points, size_t keysize, size_t outsize, size_t domainsize,
    std::shared_ptr<CommParty> channel, RandomOTReceiver rots
  );
protected:
  std::vector<std::pair<BitString, BitString>> levels;
  std::shared_ptr<std::vector<BitString>> leafs;

  size_t keysize;
  size_t domainsize;
  size_t outsize;
  size_t depth;

  // whether we've done whole domain evaluation
  bool expanded;

  // point that has been punctured
  std::vector<BitString> keys;
  std::vector<uint32_t> points;
};

// D(istributed) P(oint) F(unction) (i.e., special case of pprf where the output is binary)
class DPF {
public:
  DPF() { }

  // initialize given the root `key`
  DPF(BitString key, size_t domainsize);

  // initialize the dpf which has been punctured at `x`
  DPF(std::vector<BitString> keys, uint32_t point);

  // create `n` dpfs with randomly sampled keys
  static std::vector<DPF> sample(size_t n, size_t keysize, size_t domainsize);

  // expand a pprf that has been shared
  void expand();

  // the truth table for the function
  BitString image() const {
    if (!this->expanded) {
      throw std::runtime_error("[DPF::image()] dpf has not been expanded yet");
    }
    return (*this->_image);
  }

  size_t domain() const { return domainsize; }

  void clear() { _image.reset(); levels.clear(); keys.clear(); }

  // share across `channel` punctured according to `points` with outputs `payloads`
  static void send(
    std::vector<DPF> dpfs, BitString payloads,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
  );

  static std::vector<DPF> receive(
    std::vector<uint32_t> points, size_t keysize, size_t domainsize,
    std::shared_ptr<CommParty> channel, RandomOTReceiver rots
  );
protected:
  std::shared_ptr<BitString> _image;

  // information required to send
  std::vector<std::pair<BitString, BitString>> levels;

  // seed used to expand punctured into full image
  std::vector<BitString> keys;
  uint32_t point;

  size_t keysize;
  size_t domainsize;
  size_t depth;

  // whether we've done whole domain evaluation
  bool expanded;
};
