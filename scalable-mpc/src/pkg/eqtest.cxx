#include "pkg/eqtest.hpp"

#include <cmath>

#include "util/random.hpp"

uint32_t EqTest::numOTs(uint32_t length, int threshold, size_t tests) {
  uint32_t ots = 0;
  for (size_t j = length; j > threshold; j = (size_t) ceil(log2(j + 1))) {
    ots += (tests * j); // used in size reduction

  }
  return ots + ((1 << threshold) - 2) * tests; // used in product sharing
}

void EqTest::init() {
  for (size_t j = this->length; j > this->threshold; j = (size_t) ceil(log2(j + 1))) {
    this->sizeReduction(j);
  }
  this->productSharing();
}

BitString EqTest::run(std::vector<uint32_t> inputs) {
  // we've prepared for exactly `tests` inputs
  assert(inputs.size() == this->tests);

  std::vector<BitString> reduced(inputs.size());
  for (size_t t = 0; t < this->tests; t++) {
    reduced[t] = BitString::fromUInt(inputs[t], this->length);
  }

  for (size_t i = 0, j = this->length; j > this->threshold; i++, j = (size_t) ceil(log2(j + 1))) {
    BitString x;
    for (size_t t = 0; t < this->tests; t++) {
      x += reduced[t] ^ rsi[t][i];
    }
    BitString y(x.size());

    if (sender) {
      channel->write(x.data(), x.nBytes());
      channel->read(y.data(), y.nBytes());
    } else {
      channel->read(y.data(), y.nBytes());
      channel->write(x.data(), x.nBytes());
    }

    // compute next iteration of `x`
    BitString z = x ^ y;
    for (size_t t = 0; t < this->tests; t++) {
      BitString zt = z[{j * t, j * (t + 1)}];
      inputs[t] = 0;
      for (size_t l = 0; l < j; l++) {
        inputs[t] = (inputs[t] + (zt[l] ? (j + 1) - abi[t][i][l] : abi[t][i][l])) % (j + 1);
      }

      // Alice negates their share
      if (sender) { inputs[t] = ((j + 1) - inputs[t]) % (j + 1); }

      // Bob adds z_i[l] for all l
      else { inputs[t] = (inputs[t] + zt.weight()) % (j + 1); }

      reduced[t] = BitString::fromUInt(inputs[t], (size_t) ceil(log2(j + 1)));
    }
  }

  std::vector<BitString> X(inputs.size());
  BitString alpha;
  try {
    for (size_t t = 0; t < this->tests; t++) {
      X[t] = BitString((1 << this->threshold) - 2);
      for (size_t k = 1; k < (1 << this->threshold) - 1; k++) {
        bool Xk = true;
        for (size_t l = 0; l < this->threshold; l++) {
          bool in_subset = (k & (1 << l));
          if (sender && in_subset) {
            Xk &= true ^ reduced[t][l];
          } else if (!sender && !in_subset) {
            Xk &= reduced[t][l];
          }
        }
        X[t][k - 1] = Xk;
      }

      alpha += X[t] ^ this->rs[t];
    }
  } catch (const std::out_of_range& err)  {
    // TODO: There is a bug that occurs when the reduced length != threshold.
    // It's not entirely clear how this should be resolved as the paper doesn't seem
    // to address it. I suspect you just use the reduced length rather than the
    // threadshold for the rest of the computation, but that needs to be tested.
    //
    // This can be reproduced by using (length = 8, threshold = 5)
    throw std::runtime_error("[EqTest] threshold error");
  }

  // the index where the random bits start
  size_t rbits = alpha.size();

  // randomly sampled bits; just called \alpha in the paper
  alpha += BitString::sample(inputs.size());

  BitString beta(alpha.size());

  if (sender) {
    channel->write(alpha.data(), alpha.nBytes());
    channel->read(beta.data(), beta.nBytes());
  } else {
    channel->read(beta.data(), beta.nBytes());
    channel->write(alpha.data(), alpha.nBytes());
  }

  // assemble output
  BitString output(inputs.size());
  for (size_t t = 0; t < this->tests; t++) {
    output[t] = true;
    for (size_t l = 0; l < this->threshold; l++) {
      output[t] &= sender ? true ^ reduced[t][l] : reduced[t][l];
    }
    // start bit for this input's beta
    size_t startbit = t * ((1 << this->threshold) - 2);

    for (size_t k = 0; k < (1 << this->threshold) - 2; k++) {
      output[t] ^= ab[t][k] ^ (beta[startbit + k] & (sender ? X[t][k] : rs[t][k]));
    }
    output[t] ^= alpha[rbits + t] ^ beta[rbits + t];
  }

  return output;
}

////////////////////////////////////////////////////////////////////////////////
// SIZE REDUCTION
////////////////////////////////////////////////////////////////////////////////

void EqTestSender::sizeReduction(uint32_t size) {
  std::vector<std::pair<BitString, BitString>> messages;

  for (size_t t = 0; t < this->tests; t++) {
    BitString x = BitString::sample(size);
    vector<uint32_t> a = sampleVector(size, size + 1);

    size_t mbits = (size_t) ceil(log2(size + 1));

    for (uint32_t i = 0; i < size; i++) {
      // modular arithmetic makes this look complicated
      uint32_t m0 = x[i] >= a[i] ? x[i] - a[i] : size + 1 + x[i] - a[i];
      uint32_t m1 = 1 - x[i] + (1 - x[i] >= a[i] ? -a[i] : size + 1 - a[i]);

      messages.push_back(
        std::make_pair(BitString::fromUInt(m0, mbits), BitString::fromUInt(m1, mbits))
      );
    }

    this->rsi[t].push_back(x);
    this->abi[t].push_back(a);
  }

  // do the oblivious transfer
  this->rots.transfer(messages, this->channel);
}

void EqTestReceiver::sizeReduction(uint32_t size) {
  BitString y = BitString::sample(size * tests);

  size_t mbits = (size_t) ceil(log2(size + 1));

  // do the oblivious transfer
  std::vector<BitString> my = this->rots.transfer(y, mbits, this->channel);

  for (size_t t = 0; t < this->tests; t++) {
    this->rsi[t].push_back(y[{t * size, (t + 1) * size}]);

    std::vector<uint32_t> b;
    for (size_t i = 0; i < size; i++) {
      b.push_back(my[(t * size) + i].toUInt());
    }
    this->abi[t].push_back(b);
  }
}

////////////////////////////////////////////////////////////////////////////////
// PRODUCT SHARING
////////////////////////////////////////////////////////////////////////////////

void EqTestSender::productSharing() {
  size_t bits = (1 << this->threshold) - 2;
  BitString x;
  BitString a;
  for (size_t t = 0; t < tests; t++) {
    this->rs[t] = BitString::sample(bits);
    this->ab[t] = BitString::sample(bits);
    x += this->rs[t];
    a += this->ab[t];
  }

  this->rots.transfer(a, a ^ x, this->channel);
}

void EqTestReceiver::productSharing() {
  size_t bits = (1 << this->threshold) - 2;
  BitString y = BitString::sample(bits * tests);
  BitString b = this->rots.transfer(y, this->channel);

  for (size_t t = 0; t < this->tests; t++) {
    this->rs[t] = y[{t * bits, (t + 1) * bits}];
    this->ab[t] = b[{t * bits, (t + 1) * bits}];
  }
}
