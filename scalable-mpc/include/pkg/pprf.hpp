#pragma once

#include <utility>
#include <tuple>

#include <libscapi/include/comm/Comm.hpp>

#include "pkg/rot.hpp"
#include "util/bitstring.hpp"

// P(unctured) P(seudo)R(andom) F(unction)
class PPRF {
public:
  // initialize unpunctured with the given `key`
  PPRF(BitString key, size_t outsize, size_t domainsize);

  // initialize punctured at `x`
  PPRF(std::vector<BitString> keys, uint32_t x, size_t outsize);

  // create `n` pprfs with randomly sampled keys
  static std::vector<PPRF> sample(size_t n, size_t keysize, size_t outsize, size_t domainsize);

  // evaluate the pprf on `x`
  BitString operator() (uint32_t x) const;

  size_t domain() const { return domainsize; }

  // free up the internal nodes (once they are not needed for puncturing)
  void compress();

  // for debugging purposes
  std::string toString() const;

  // share across `channel` where the other party is puncturing at a secret index with `payload`
  static void send(
    PPRF pprf, BitString payload, std::shared_ptr<CommParty> channel, RandomOTSender rots
  );

  // batch sending
  static void send(
    std::vector<PPRF> pprfs, std::vector<BitString> payloads,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
  );

  // batch sending with the same payload
  static void send(
    std::vector<PPRF> pprfs, BitString payload,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
  );

  // receive a pprf over `channel` and puncture at `x`
  static PPRF receive(
      uint32_t x, size_t keysize, size_t outsize, size_t domainsize,
      std::shared_ptr<CommParty> channel, RandomOTReceiver rots
  );

  // batch receiving
  static std::vector<PPRF> receive(
      std::vector<uint32_t> points, size_t keysize, size_t outsize, size_t domainsize,
      std::shared_ptr<CommParty> channel, RandomOTReceiver rots
  );
protected:
  vector<BitString> tree;

  size_t keysize;
  size_t domainsize;
  size_t outsize;
  size_t depth;

  bool compressed = false;

  // get the left / right nodes on level `l` xor'd together
  BitString getLeftXORd(size_t l) const;
  BitString getRightXORd(size_t l) const;
  std::pair<BitString, BitString> getLevelXORd(size_t l) const;

  // get the index in tree for the `i`th node on the `l`th level
  size_t getIndex(size_t l, size_t i) { return (1 << l) - 1 + i; }

  // check if an index `i` is a leaf node
  size_t isLeaf(size_t i) {
    if (compressed) { return true; }
    return i >= (1 << this->depth) - 1;
  }

  // fill out the tree rooted at index (e.g., fill(0) would populate the whole tree)
  void fill(size_t index);
};

// D(istributed) P(oint) F(unction) (i.e., special case of pprf where the output is binary)
class DPF : public PPRF {
public:
  DPF(BitString key, size_t domainsize) : PPRF(key, 1, domainsize) { }
  DPF(std::vector<BitString> keys, uint32_t x) : PPRF(keys, x, 1) { }
  static std::vector<DPF> sample(size_t n, size_t keysize, size_t domainsize);

  // the truth table for the function
  BitString image() const;

  static void send(
    std::vector<DPF> dpfs, BitString payloads,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
  );

  static std::vector<DPF> receive(
      std::vector<uint32_t> points, size_t keysize, size_t domainsize,
      std::shared_ptr<CommParty> channel, RandomOTReceiver rots
  );
};
