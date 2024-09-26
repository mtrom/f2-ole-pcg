#include <stack>

#include "pkg/pprf.hpp"
#include "ahe/ahe.hpp"
#include "util/concurrency.hpp"


PPRF::PPRF(BitString key, size_t outsize, size_t domainsize)
  : keysize(key.size()), domainsize(domainsize), outsize(outsize),
    depth((size_t) ceil(log2(domainsize)))
{
  std::vector<BitString> seed({key});
  this->leafs = std::make_shared<std::vector<BitString>>(seed);
  this->levels.resize(depth + 1);

  // expand the tree but only hold onto the last level evaluated
  for (size_t l = 0; l < depth; l++) {
    auto next = std::make_shared<std::vector<BitString>>(this->leafs->size() * 2);

    // concurrently expand out the level at this depth
    auto xors = TASK_REDUCE<std::pair<BitString, BitString>>(
      [this, &l, &next](size_t start, size_t end)
    {
      BitString left(this->keysize), right(this->keysize);
      for (size_t i = start; i < end; i++) {
        PRF<BitString> prf((*this->leafs)[i]);
        (*next)[2 * i] = prf(0, this->keysize);
        (*next)[2 * i + 1] = prf(1, this->keysize);

        left ^= (*next)[2 * i];
        right ^= (*next)[2 * i + 1];
      }
      return std::make_pair(left, right);
    }, BitString::xor_combine, this->leafs->size());

    // save the left and right xors
    this->levels[l] = std::make_pair(xors.first, xors.second);
    this->leafs = next;
  }

  // expand leafs to the specified `outsize`
  auto leaf_xors = TASK_REDUCE<std::pair<BitString, BitString>>(
    [this](size_t start, size_t end)
  {
    BitString left(this->outsize), right(this->outsize);
    for (size_t i = start; i < end; i++) {
      PRF<BitString> prf((*this->leafs)[i]);
      (*this->leafs)[i] = prf(0, this->outsize);
      if (i % 2 == 0) { left ^= (*this->leafs)[i]; }
      else            { right ^= (*this->leafs)[i]; }
    }
    return std::make_pair(left, right);
  }, BitString::xor_combine, this->leafs->size());

  this->levels[this->depth] = std::make_pair(leaf_xors.first, leaf_xors.second);

  this->expanded = true;
}

PPRF::PPRF(
  std::vector<BitString> keys, std::vector<uint32_t> points, size_t outsize, size_t domainsize
) : keys(keys), keysize(keys[0].size()), domainsize(domainsize), outsize(outsize),
    depth((size_t) ceil(log2(domainsize))), expanded(false), points(points) { }

void PPRF::expand() {
  for (size_t i = 0; i < points.size(); i++) {
    std::vector<BitString> seed({BitString()});
    auto previous = std::make_shared<std::vector<BitString>>(seed);

    BitString path = BitString::fromUInt(points[i], depth).reverse();

    for (size_t l = 0; l < depth; l++) {
      auto next = std::make_shared<std::vector<BitString>>(previous->size() * 2);
      size_t sibling;

      // concurrently expand out the level at this depth
      auto xors = TASK_REDUCE<std::pair<BitString, BitString>>(
        [this, &path, &previous, &l, &next, &sibling](size_t start, size_t end)
      {
        BitString left(this->keysize), right(this->keysize);
        for (size_t j = start; j < end; j++) {
          // if at the puncture point just insert the key for that
          if ((*previous)[j].size() == 0 && !path[l]) {
            sibling = 2 * j + 1;
          } else if ((*previous)[j].size() == 0 && path[l]) {
            sibling = 2 * j;
          } else {
            PRF<BitString> prf((*previous)[j]);
            (*next)[2 * j] = prf(0, this->keysize);
            (*next)[2 * j + 1] = prf(1, this->keysize);

            left ^= (*next)[2 * j];
            right ^= (*next)[2 * j + 1];
          }
        }
        return std::make_pair(left, right);
      }, BitString::xor_combine, previous->size());

      // for inner nodes place the new key in the sibling position
      (*next)[sibling] = this->keys[(i * (depth + 1)) + l] ^ (path[l] ? xors.first : xors.second);
      previous = next;
    }

    // expand non-punctured leafs to the specified `outsize`
    auto leaf_xors = TASK_REDUCE<std::pair<BitString, BitString>>(
      [this, &previous](size_t start, size_t end)
    {
      BitString left(this->outsize), right(this->outsize);
      for (size_t j = start; j < end; j++) {
        if ((*previous)[j].size() == 0) { continue; }
        PRF<BitString> prf((*previous)[j]);
        (*previous)[j] = prf(0, this->outsize);
        if (j % 2 == 0) { left ^= (*previous)[j]; }
        else            { right ^= (*previous)[j]; }
      }
      return std::make_pair(left, right);
    }, BitString::xor_combine, previous->size());

    // put the punctured output where it should be
    (*previous)[points[i]] = (
      this->keys[(i * (depth + 1)) + depth]
      ^ (points[i] % 2 == 0 ? leaf_xors.first : leaf_xors.second)
    );

    // xor this with the current state of the pprf
    if (this->leafs.get() == nullptr) {
      this->leafs = previous;
    } else {
      MULTI_TASK([this, &previous](size_t start, size_t end) {
        for (size_t j = start; j < end; j++) {
          (*this->leafs)[j] ^= (*previous)[j];
        }
      }, this->domainsize);
    }
  }

  this->expanded = true;
}

PPRF PPRF::sample(size_t n, size_t keysize, size_t outsize, size_t domainsize) {
  PPRF pprf = PPRF(BitString::sample(keysize), outsize, domainsize);
  for (size_t i = 1; i < n; i++) {
    pprf ^= PPRF(BitString::sample(keysize), outsize, domainsize);
  }
  return pprf;
}

std::vector<PPRF> PPRF::sampleMany(size_t n, size_t keysize, size_t outsize, size_t domainsize) {
  std::vector<PPRF> pprfs;
  for (size_t i = 0; i < n; i++) {
    pprfs.push_back(PPRF(BitString::sample(keysize), outsize, domainsize));
  }
  return pprfs;
}

BitString PPRF::operator() (uint32_t x) const {
  if (x >= domainsize) {
    throw std::out_of_range("[PPRF::operator()] x not in domain (x = " + std::to_string(x) + ")");
  } else if (!this->expanded) {
    throw std::runtime_error("[PPRF::operator()] pprf has not been expanded yet");
  }
  return (*this->leafs)[x];
}

PPRF& PPRF::operator^=(const PPRF& other) {
  if (other.domainsize != this->domainsize) {
    throw std::runtime_error("[PPRF::operator=^] other pprf has different domainsize");
  } else if (other.keysize != this->keysize) {
    throw std::runtime_error("[PPRF::operator=^] other pprf has different keysize");
  } else if (other.outsize != this->outsize) {
    throw std::runtime_error("[PPRF::operator=^] other pprf has different outsize");
  } else if (!other.expanded || !this->expanded) {
    throw std::runtime_error("[PPRF::operator=^] trying to xor a pprf that isn't expanded");
  }

  // combine information for sending
  this->levels.insert(this->levels.end(), other.levels.begin(), other.levels.end());

  MULTI_TASK([this, &other](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      (*this->leafs)[i] ^= (*other.leafs)[i];
    }
  }, this->domainsize);

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
// SHARING
////////////////////////////////////////////////////////////////////////////////

void PPRF::send(
  PPRF pprf, BitString payload, std::shared_ptr<CommParty> channel, RandomOTSender rots
) {
  if (payload.size() != pprf.outsize) {
    throw std::invalid_argument("[PPRF::send()] payload size does not match pprf output size");
  }

  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < pprf.levels.size(); i++) {
    if (i % (pprf.depth + 1) == pprf.depth) {
      // leaf level is xor'd with payload
      messages.push_back(std::make_pair(
        pprf.levels[i].first ^ payload, pprf.levels[i].second ^ payload
      ));
    } else {
      messages.push_back(pprf.levels[i]);
    }
  }

  // do the oblivious transfer
  rots.transfer(messages, channel);
}

PPRF PPRF::receive(
  std::vector<uint32_t> points, size_t keysize, size_t outsize, size_t domainsize,
  std::shared_ptr<CommParty> channel, RandomOTReceiver rots
) {
  size_t depth = (size_t) ceil(log2(domainsize));
  BitString choices;

  std::vector<size_t> sizes;

  for (const size_t& x : points) {
    // want the sibling nodes for the path to `x`
    choices += BitString::fromUInt(~x, depth).reverse();

    // except the payload which should be the punctured leaf node
    choices += (!choices[choices.size() - 1]);

    // most of the ots are `keysize`-bit but the last is the `outsize` for the leaf layer
    std::vector<size_t> branches(depth, keysize);
    sizes.insert(sizes.end(), branches.begin(), branches.end());
    sizes.push_back(outsize);
  }

  // do the oblivious transfer
  std::vector<BitString> keys = rots.transfer(choices, sizes, channel);

  return PPRF(keys, points, outsize, domainsize);
}

////////////////////////////////////////////////////////////////////////////////
// DPF / D2PF SPECIFIC FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

DPF::DPF(BitString key, size_t domainsize)
  : keysize(key.size()), domainsize(domainsize), depth((size_t) ceil(log2(domainsize)))
{
  std::vector<BitString> seed({key});
  auto previous = std::make_shared<std::vector<BitString>>(seed);
  this->levels.resize(depth);

  // expand the tree but only hold onto the last level evaluated
  for (size_t l = 0; l < depth; l++) {
    auto next = std::make_shared<std::vector<BitString>>(previous->size() * 2);
    size_t nodesize = (l == depth - 1) ? 1 : this->keysize;

    BitString left(nodesize);
    BitString right(nodesize);
    for (size_t i = 0; i < previous->size(); i++) {
      PRF<BitString> prf((*previous)[i]);
      (*next)[2 * i] = prf(0, nodesize);
      (*next)[2 * i + 1] = prf(1, nodesize);

      left ^= (*next)[2 * i];
      right ^= (*next)[2 * i + 1];
    }

    // save the left and right xors
    this->levels[l] = std::make_pair(left, right);
    previous = next;
  }

  // compresses leaf nodes into a single BitString
  this->_image = std::make_shared<BitString>(domainsize);
  for (size_t i = 0; i < domainsize; i++) {
    (*this->_image)[i] = (*previous)[i][0];
  }

  this->expanded = true;
}

DPF::DPF(std::vector<BitString> keys, uint32_t point)
  : keys(keys), point(point), keysize(keys[0].size()), domainsize(1 << keys.size()),
    depth((size_t) ceil(log2(domainsize))), expanded(false) { }

void DPF::expand() {
  std::vector<BitString> seed({BitString()});
  auto previous = std::make_shared<std::vector<BitString>>(seed);

  BitString path = BitString::fromUInt(this->point, this->depth).reverse();

  for (size_t l = 0; l < depth; l++) {
    auto next = std::make_shared<std::vector<BitString>>(previous->size() * 2);
    size_t nodesize = (l == depth - 1) ? 1 : this->keysize;
    size_t sibling;

    BitString left(nodesize);
    BitString right(nodesize);
    for (size_t i = 0; i < previous->size(); i++) {
      // if at the puncture point just insert the key for that
      if ((*previous)[i].size() == 0 && !path[l]) {
        sibling = 2 * i + 1;
      } else if ((*previous)[i].size() == 0 && path[l]) {
        sibling = 2 * i;
      } else {
        PRF<BitString> prf((*previous)[i]);
        (*next)[2 * i] = prf(0, nodesize);
        (*next)[2 * i + 1] = prf(1, nodesize);

        left ^= (*next)[2 * i];
        right ^= (*next)[2 * i + 1];
      }
    }

    // for inner nodes just place the new key in the sibling position
    if (l < depth - 1) {
      (*next)[sibling] = this->keys[l] ^ (path[l] ? left : right);
    }
    // at the leafs insert both the sibling and the puncture point
    else {
      left[0] ^= this->keys[l][0];
      right[0] ^= this->keys[l][1];
      (*next)[sibling] = path[l] ? left : right;
      (*next)[this->point] = !path[l] ? left : right;
    }

    previous = next;
  }

  // compresses leaf nodes into a single BitString
  this->_image = std::make_shared<BitString>(domainsize);
  for (size_t i = 0; i < domainsize; i++) {
    (*this->_image)[i] = (*previous)[i][0];
  }

  this->expanded = true;
}

std::vector<DPF> DPF::sample(size_t n, size_t keysize, size_t domainsize) {
  std::vector<DPF> dpfs(n);

  MULTI_TASK([&dpfs, &keysize, &domainsize](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      dpfs[i] = DPF(BitString::sample(keysize), domainsize);
    }
  }, n);
  return dpfs;
}

void DPF::send(
  std::vector<DPF> dpfs, BitString payloads,
  std::shared_ptr<CommParty> channel, RandomOTSender rots
) {
  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < dpfs.size(); i++) {
    // the messages are the level left / right xors for the branch levels
    messages.insert(messages.end(), dpfs[i].levels.begin(), dpfs[i].levels.end() - 1);

    // leaf level is sent with both where payload is xor'd
    std::pair<BitString, BitString> leafs = dpfs[i].levels.back();
    std::pair<BitString, BitString> final_msg = std::make_pair(
      leafs.first + leafs.second, leafs.first + leafs.second
    );
    final_msg.first[1] ^= payloads[i];
    final_msg.second[0] ^= payloads[i];
    messages.push_back(final_msg);
  }

  // do the oblivious transfer
  rots.transfer(messages, channel);
}

std::vector<DPF> DPF::receive(
    std::vector<uint32_t> points, size_t keysize, size_t domainsize,
    std::shared_ptr<CommParty> channel, RandomOTReceiver rots
) {
  size_t depth = (size_t) ceil(log2(domainsize));
  BitString choices;

  std::vector<size_t> sizes;

  for (const size_t& x : points) {
    // want the sibling nodes for the path to `x`
    choices += BitString::fromUInt(~x, depth).reverse();

    // most of the ots are `keysize`-bit but the last two are bits for the leaf layer
    std::vector<size_t> branches(depth - 1, keysize);
    sizes.insert(sizes.end(), branches.begin(), branches.end());
    sizes.push_back(2);
  }

  // do the oblivious transfer
  std::vector<BitString> allkeys = rots.transfer(choices, sizes, channel);

  std::vector<DPF> dpfs;
  for (size_t i = 0; i < points.size(); i++) {
    std::vector<BitString> keys(
      allkeys.begin() + i * depth,
      allkeys.begin() + (i + 1) * depth
    );
    dpfs.push_back(DPF(keys, points[i]));
  }

  return dpfs;
}
