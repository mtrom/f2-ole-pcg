#include <gtest/gtest.h>

// allows us to test private methods
#define protected public
#define private public

#include "pkg/pcg.hpp"
#include "./fixtures.cxx"

#include "util/defines.hpp"

class PCGTests : public NetworkTest { };

// insecure but small params to test with
PCGParams TEST_PARAMS(
  BitString::sample(LAMBDA), 1 << 16, 1 << 10, 1 << 8, 1 << 3,
  BitString::sample(LAMBDA), 1 << 2, 1 << 5
);

TEST_F(PCGTests, Transform) {
  const size_t N = 128;
  const size_t OUTPUT_SIZE = 1024;

  // create all the parties
  std::vector<Beaver::MockPCG> parties;
  for (size_t id = 0; id < N; id++) {
    parties.push_back(Beaver::MockPCG(id, OUTPUT_SIZE));
  }

  // "run" the 2-party protocol between all parties
  std::vector<Beaver::Triples> output;
  for (size_t i = 0; i < N; i++) {
    std::vector<BitString> c, d, seeds;
    for (size_t j = 0; j < N; j++) {
      if (i == j) { continue; }
      BitString cij, dji, seed;
      std::tie(seed, cij, dji) = parties[i].run(j);
      c.push_back(cij);
      d.push_back(dji);
      seeds.push_back(seed);
    }
    BitString a, b;
    std::tie(a, b) = parties[i].inputs();
    output.push_back(Beaver::transform(a, b, c, d, seeds));
  }


  // we've gotten the correct number of correlations
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(output[i].remaining(), OUTPUT_SIZE);
  }

  // those correlations are correct
  while (output[0].remaining() > 0) {
    bool a = false, b = false, c = false;
    for (size_t id = 0; id < N; id++) {
      auto triple = output[id].get();
      a ^= triple.a;
      b ^= triple.b;
      c ^= triple.c;
    }
    ASSERT_EQ((bool) (a & b), c);
  }
}

TEST_F(PCGTests, SecretTensor) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 6, 1 << 5, 1 << 3, 1 << 3,
    BitString::sample(LAMBDA), 4, 7
  );
  Beaver::Sender sender(params);
  Beaver::Receiver receiver(params);

  auto results = this->launch(
    [&]() -> BitString {
      return sender.secretTensor(this->sch, this->srots);
    },
    [&]() -> BitString {
      return receiver.secretTensor(this->rch, this->rrots);
    }
  );
  BitString actual = results.first ^ results.second;

  // figure out what the actual ⟨bᵢ⊗ aᵢ,ε ⊗ s⟩ term should be
  BitString expected(params.size);

  LPN::PrimalMatrix A = sender.A;
  LPN::DenseMatrix B = sender.B;
  BitString s = sender.s;

  // reconstruct the epsilon vector with `params.dual.t` errors
  BitString epsilon(params.dual.N());
  for (size_t i = 0; i < params.dual.t; i++) {
    epsilon[receiver.epsilon[i]] = true;
  }

  BitString eXs = epsilon.tensor(s);
  for (size_t i = 0; i < actual.size(); i++) {
    expected[i] = B[i].tensor(A[i]) * eXs;
  }

  ASSERT_EQ(expected.size(), actual.size());
  ASSERT_EQ(expected, actual);
}

TEST_F(PCGTests, InnerProduct) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 6, 1 << 5, 1 << 3, 1 << 3,
    BitString::sample(LAMBDA), 4, 7
  );
  Beaver::Sender sender(params);
  Beaver::Receiver receiver(params);

  auto results = this->launch(
    [&]() -> BitString {
      return sender.sendInnerProductTerm(this->sch, this->srots);
    },
    [&]() -> BitString {
      return receiver.receiveInnerProductTerm(this->rch, this->rrots);
    }
  );
  BitString actual = results.first ^ results.second;

  // figure out what the actual ⟨aᵢ,s⟩ term should be
  LPN::PrimalMatrix A = sender.A;
  BitString s = sender.s;

  // reconstruct the primal error vector
  BitString e(params.primal.n);
  for (size_t i = 0; i < params.primal.t; i++) {
    e[(i * params.primal.blockSize()) + receiver.e[i]] = true;
  }

  BitString aXs = A * s;
  BitString expected = aXs & e;

  ASSERT_EQ(expected.size(), actual.size());
  ASSERT_EQ(expected.toString(), actual.toString());
}

TEST_F(PCGTests, ErrorsProduct) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 11, 1 << 5, 1 << 3, 1 << 3,
    BitString::sample(LAMBDA), 1 << 2, 1 << 3
  );
  Beaver::Sender sender(params);
  Beaver::Receiver receiver(params);

  // set errors explicitly so we can be sure of some equalities
  sender.e = std::vector<uint32_t>{26, 9, 59, 71, 18, 87, 17, 89};
  receiver.e = std::vector<uint32_t>{44, 9, 70, 65, 87, 74, 28, 89};

  auto results = this->launch(
    [&]() -> BitString {
      return sender.errorProduct(this->sch, this->srots);
    },
    [&]() -> BitString {
      return receiver.errorProduct(this->rch, this->rrots);
    }
  );
  BitString sout, rout;
  std::tie(sout, rout) = results;

  ASSERT_EQ(sout.size(), params.size);
  ASSERT_EQ(rout.size(), params.size);

  for (size_t i = 0, k = 0 ; i < params.primal.t; i++) {
    for (size_t j = 0; j < params.primal.blockSize(); j++, k++) {
      if (j == sender.e[i] && sender.e[i] == receiver.e[i]) {
        ASSERT_EQ(sout[k] ^ rout[k], true);
      } else {
        ASSERT_EQ(sout[k] ^ rout[k], false);
      }
    }
  }
}

TEST_F(PCGTests, SenderReceiverRun) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 12, 1 << 7, 1 << 6, 1 << 3,
    BitString::sample(LAMBDA), 4, 7
  );
  Beaver::Sender sender(params);
  Beaver::Receiver receiver(params);

  RandomOTSender sender_srots = this->srots.reserve(1 << 10);
  RandomOTReceiver receiver_rrots = this->rrots.reserve(1 << 10);
  RandomOTReceiver sender_rrots = this->rrots.reserve(1 << 10);
  RandomOTSender receiver_srots = this->srots.reserve(1 << 10);

  auto results = this->launch(
    [&]() -> BitString {
      return sender.run(this->sch, sender_srots, sender_rrots);
    },
    [&]() -> BitString {
      return receiver.run(this->rch, receiver_srots, receiver_rrots);
    }
  );

  BitString a = sender.lpnOutput();
  BitString b = receiver.lpnOutput();
  BitString c = results.first ^ results.second;
  ASSERT_EQ(a & b, c);
}

TEST_F(PCGTests, PCGRun) {
  PCGParams params(
    BitString::sample(LAMBDA), 1 << 12, 1 << 7, 1 << 6, 1 << 3,
    BitString::sample(LAMBDA), 4, 7
  );
  const uint32_t ALICE_ID = 0, BOB_ID = 1;

  Beaver::PCG alice(ALICE_ID, params);
  Beaver::PCG bob(BOB_ID, params);

  RandomOTSender sender_srots = this->srots.reserve(1 << 10);
  RandomOTReceiver receiver_rrots = this->rrots.reserve(1 << 10);
  RandomOTReceiver sender_rrots = this->rrots.reserve(1 << 10);
  RandomOTSender receiver_srots = this->srots.reserve(1 << 10);

  auto results = this->launch(
    [&]() -> std::pair<BitString, BitString> {
      return alice.run(BOB_ID, this->sch, sender_srots, sender_rrots);
    },
    [&]() -> std::pair<BitString, BitString> {
      return bob.run(ALICE_ID, this->rch, receiver_srots, receiver_rrots);
    }
  );

  // 2-party correlation inputs
  BitString a0, a1, b0, b1;
  std::tie(a0, b0) = alice.inputs();
  std::tie(a1, b1) = bob.inputs();

  // 2-party correlation outputs
  BitString c0, c1, d0, d1;
  std::tie(c0, d0) = results.first;
  std::tie(c1, d1) = results.second;

  ASSERT_EQ(a0 & b1, c0 ^ c1);
  ASSERT_EQ(b0 & a1, d0 ^ d1);
}

TEST_F(PCGTests, MockSenderReceiver) {
  BitString skey = BitString::sample(LAMBDA);
  BitString rkey = BitString::sample(LAMBDA);
  const size_t OUTPUT_SIZE = 1024;

  Beaver::MockSender sender(OUTPUT_SIZE, skey);
  Beaver::MockReceiver receiver(OUTPUT_SIZE, rkey);

  BitString a = sender.lpnOutput();
  BitString b = receiver.lpnOutput();
  BitString c0 = sender.run(rkey);
  BitString c1 = receiver.run(skey);

  ASSERT_EQ(a & b, c0 ^ c1);
}

TEST_F(PCGTests, MockPCG) {
  const uint32_t ALICE_ID = 0, BOB_ID = 1;
  const size_t OUTPUT_SIZE = 1024;

  Beaver::MockPCG alice(ALICE_ID, OUTPUT_SIZE);
  Beaver::MockPCG bob(BOB_ID, OUTPUT_SIZE);

  // 2-party correlation inputs
  BitString a0, a1, b0, b1;
  std::tie(a0, b0) = alice.inputs();
  std::tie(a1, b1) = bob.inputs();

  // 2-party correlation outputs
  BitString c0, c1, d0, d1, s0, s1;
  std::tie(s0, c0, d0) = alice.run(BOB_ID);
  std::tie(s1, c1, d1) = bob.run(ALICE_ID);

  ASSERT_EQ(s0, s1);
  ASSERT_EQ(a0 & b1, c0 ^ c1);
  ASSERT_EQ(b0 & a1, d0 ^ d1);
}
