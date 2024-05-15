#pragma once

#include "pkg/pcg.hpp"
#include "pkg/rot.hpp"
#include "pkg/svole.hpp"
#include "util/circuit.hpp"
#include "util/params.hpp"

class Evaluator {
public:
  Evaluator(
    std::vector<std::shared_ptr<CommParty>> channels, Beaver::Triples triples, sVOLE svoles
  ) : channels(channels), triples(triples), svoles(svoles) { }

  BitString run(Circuit circuit, const BitString& input);
protected:
  // communication channels set up with each garbler
  std::vector<std::shared_ptr<CommParty>> channels;

  // random ots set up with each garbler
  std::vector<RandomOTReceiver> rots;

  // n-party correlations
  Beaver::Triples triples;
  sVOLE svoles;
};

class Garbler {
public:
  Garbler(
    uint16_t id, uint32_t parties, std::shared_ptr<CommParty> channel,
    Beaver::Triples triples, sVOLE svoles
  ) : id(id), parties(parties), channel(channel), triples(triples), svoles(svoles) { }

  void run(Circuit circuit, const BitString& input);
protected:
  // party identifier
  uint16_t id;

  // number of parties in the protocol
  uint32_t parties;

  // communication channel to the evaluator
  std::shared_ptr<CommParty> channel;

  // random ots set up with evaluator
  RandomOTSender rots;

  // n-party correlations
  Beaver::Triples triples;
  sVOLE svoles;
};
