#pragma once

#include "pkg/rot.hpp"
#include "util/bitstring.hpp"
#include "util/defines.hpp"

//
/**
 * private equality testing using the protocol from https://eprint.iacr.org/2016/544.pdf
 *
 * translation to the notation of the paper:
 *   - \ell      = length    (input to protocol)
 *   - n         = threshold (input to protocol / product sharing)
 *   - x / y     = input     (input to protocol)
 *   - j         = size      (input to size reduction)
 *   - r_i / s_i = rsi       (output of size reduction)
 *   - a_i / b_i = abi       (output of size reduction)
 *   - r / s     = rs        (output of product sharing)
 *   - a / b     = ab        (output of product sharing)
 */
class EqTest {
public:
  EqTest(
      bool sender, uint32_t length, int threshold, size_t tests, Channel channel
  ) : sender(sender), length(length), threshold(threshold), tests(tests), channel(channel),
      rsi(tests), abi(tests), rs(tests), ab(tests) { }

  virtual void sizeReduction(uint32_t size) = 0;
  virtual void productSharing() = 0;
  BitString run(std::vector<uint32_t> input);

  // number of oblivious transfers required to run
  static uint32_t numOTs(uint32_t length, int threshold, size_t tests);

  const bool sender;
  int length;
  int threshold;
  int tests;
  Channel channel;

  // output of size reduction
  std::vector<std::vector<BitString>> rsi;
  std::vector<std::vector<std::vector<uint32_t>>> abi;

  // output of product sharing
  std::vector<BitString> rs;
  std::vector<BitString> ab;
};

// Alice in the protocol (and subprotocols)
class EqTestSender : public EqTest {
public:
  EqTestSender(
    uint32_t length, int threshold, int tests,
    Channel channel, ROT::Sender rots
  ) : EqTest(true, length, threshold, tests, channel), rots(rots) { }
  void sizeReduction(uint32_t size) override;
  void productSharing() override;
protected:
  ROT::Sender rots;
};

// Bob in the protocol (and subprotocols)
class EqTestReceiver : public EqTest {
public:
  EqTestReceiver(
    uint32_t length, int threshold, int tests,
    Channel channel, ROT::Receiver rots
  ) : EqTest(false, length, threshold, tests, channel), rots(rots) { }
  void sizeReduction(uint32_t size) override;
  void productSharing() override;
protected:
  ROT::Receiver rots;
};
