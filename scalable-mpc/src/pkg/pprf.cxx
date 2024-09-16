#include <stack>

#include "pkg/pprf.hpp"
#include "ahe/ahe.hpp"


PPRF::PPRF(BitString key, size_t outsize, size_t domainsize)
  : keysize(key.size()), domainsize(domainsize), outsize(outsize),
    depth((size_t) ceil(log2(domainsize)))
{
  this->tree.resize((1 << (this->depth + 1)) - 1);
  this->tree[0] = key;

  // expand tree to necessary depth
  for (size_t i = 0; i < tree.size() / 2; i++) {
    BitString expanded = this->tree[i].aes(key.size() * 2);
    this->tree[2 * i + 1] = expanded[{0, key.size()}];
    this->tree[2 * i + 2] = expanded[{key.size(), key.size() * 2}];
  }

  // shrink or expand leaf nodes as necessary
  if (outsize == key.size()) { return; }
  else if (outsize < key.size()) {
    for (size_t i = 0, j = (1 << this->depth) - 1; i < domainsize; i++, j++) {
      this->tree[j] = this->tree[j][{0, outsize}];
    }
  } else {
    for (size_t i = 0, j = (1 << this->depth) - 1; i < domainsize; i++, j++) {
      this->tree[j] = this->tree[j].aes(outsize);
    }
  }
}

std::vector<PPRF> PPRF::sample(size_t n, size_t keysize, size_t outsize, size_t domainsize) {
  std::vector<PPRF> pprfs;
  pprfs.reserve(n);
  for (size_t i = 0; i < n; i++) {
    pprfs.push_back(PPRF(BitString::sample(keysize), outsize, domainsize));
  }
  return pprfs;
}

BitString PPRF::operator() (uint32_t x) const {
  if (x > domainsize) {
    throw std::out_of_range("[PPRF::operator()] x not in domain (x = " + std::to_string(x) + ")");
  }

  if (compressed) { return this->tree[x]; }
  else            { return this->tree[(1 << this->depth) - 1 + x]; }
}

void PPRF::compress() {
  if (compressed) { return; }
  compressed = true;
  this->tree.erase(this->tree.begin(), this->tree.begin() + (1 << this->depth) - 1);
}

std::string PPRF::toString() const {
  std::string out = "";

  if (compressed) {
    for (size_t i = 0; i < this->tree.size(); i++) {
      out += this->tree[i].toString();
      out += " ";
    }
  } else {
    for (size_t i = 0; i <= this->depth; i++) {
      for (size_t j = 0; j < (1 << i); j++) {
        if (this->tree[(1 << i) - 1 + j].size() > 0) {
          out += this->tree[(1 << i) - 1 + j].toString();
        } else {
          out += "NULL";
        }
        out += " ";
      }
      out += "\n";
    }
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// SHARING / PUNCTURING
////////////////////////////////////////////////////////////////////////////////

PPRF::PPRF(std::vector<BitString> keys, uint32_t x, size_t outsize)
  : keysize(keys[0].size()), domainsize(1 << (keys.size() - 1)), outsize(outsize),
    depth(keys.size() - 1)
{
  this->tree.resize((1 << (this->depth + 1)) - 1);


  // follow the punctured path
  BitString path = BitString::fromUInt(x, keys.size() - 1).reverse();
  for (size_t p = 0, l = 0; l < this->depth; l++) {
    p = 2 * p + 1 + path[l];

    BitString mask;

    // the first key is not masked
    if (l == 0) { mask = BitString(keys.size() == 2 ? outsize : keysize); }
    // each other key is masked by the left or right nodes in its level
    else        { mask = path[l] ? getLeftXORd(l + 1) : getRightXORd(l + 1); }

    // place the unmasked key in the sibling node and expand to fill the tree
    size_t sibling = p + (path[l] ? -1 : 1);
    this->tree[sibling] = keys[l] ^ mask;
    fill(sibling);
  }

  // the last key is the punctured element
  BitString mask = path[this->depth - 1] ? getRightXORd(this->depth) : getLeftXORd(this->depth);
  this->tree[(1 << this->depth) - 1 + x] = keys[keys.size() - 1] ^ mask;
}

void PPRF::send(
    PPRF pprf, BitString payload,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
) {
  if (payload.size() != pprf.outsize) {
    throw std::invalid_argument("[PPRF::send] payload size does not match pprf output size");
  }

  // for each level the messages are the left and right nodes xor'd
  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t d = 1; d <= pprf.depth; d++) {
    messages.push_back(pprf.getLevelXORd(d));
  }

  // xor the final messages with the payload
  std::pair<BitString, BitString> leafs = pprf.getLevelXORd(pprf.depth);
  leafs.first ^= payload;
  leafs.second ^= payload;
  messages.push_back(leafs);

  // do the oblivious transfer
  rots.transfer(messages, channel);
}

PPRF PPRF::receive(
    uint32_t x, size_t keysize, size_t outsize, size_t domainsize,
    std::shared_ptr<CommParty> channel, RandomOTReceiver rots
) {
  size_t depth = (size_t) ceil(log2(domainsize));

  // want the sibling nodes for the path to `x`
  BitString choices = BitString::fromUInt(~x, depth).reverse();

  // except the payload which should be the punctured leaf node
  choices += (!choices[depth - 1]);

  // most of the ots are `keysize`-bit but the last two are `outsize` for the leaf layer
  std::vector<size_t> sizes(depth - 1, keysize);
  sizes.insert(sizes.end(), {outsize, outsize});

  // do the oblivious transfer
  std::vector<BitString> keys = rots.transfer(choices, sizes, channel);

  return PPRF(keys, x, outsize);
}

void PPRF::send(
    std::vector<PPRF> pprfs, std::vector<BitString> payloads,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
) {
  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < pprfs.size(); i++) {
    if (payloads[i].size() != pprfs[i].outsize) {
      throw std::invalid_argument(
        "[PPRF::send(std::vector, std::vector, ...)] payload size does not match pprf output size"
      );
    }

    // for each level the messages are the left and right nodes xor'd
    for (size_t d = 1; d <= pprfs[i].depth; d++) {
      messages.push_back(pprfs[i].getLevelXORd(d));
    }

    // xor the final messages with the payload
    std::pair<BitString, BitString> leafs = pprfs[i].getLevelXORd(pprfs[i].depth);
    leafs.first ^= payloads[i];
    leafs.second ^= payloads[i];
    messages.push_back(leafs);
  }

  // do the oblivious transfer
  rots.transfer(messages, channel);
}

void PPRF::send(
    std::vector<PPRF> pprfs, BitString payload,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
) {
  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < pprfs.size(); i++) {
    if (payload.size() != pprfs[i].outsize) {
      throw std::invalid_argument(
        "[PPRF::send(std::vector, BitString, ...)] payload size does not match pprf output size"
      );
    }

    // for each level the messages are the left and right nodes xor'd
    for (size_t d = 1; d <= pprfs[i].depth; d++) {
      messages.push_back(pprfs[i].getLevelXORd(d));
    }

    // xor the final messages with the payload
    std::pair<BitString, BitString> leafs = pprfs[i].getLevelXORd(pprfs[i].depth);
    leafs.first ^= payload;
    leafs.second ^= payload;
    messages.push_back(leafs);
  }

  // do the oblivious transfer
  rots.transfer(messages, channel);
}

std::vector<PPRF> PPRF::receive(
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

    // most of the ots are `keysize`-bit but the last two are `outsize` for the leaf layer
    std::vector<size_t> branches(depth - 1, keysize);
    sizes.insert(sizes.end(), branches.begin(), branches.end());
    sizes.insert(sizes.end(), {outsize, outsize});
  }

  // do the oblivious transfer
  std::vector<BitString> allkeys = rots.transfer(choices, sizes, channel);

  std::vector<PPRF> pprfs;
  for (size_t i = 0; i < points.size(); i++) {
    std::vector<BitString> keys(
      allkeys.begin() + i * (depth + 1),
      allkeys.begin() + (i + 1) * (depth + 1)
    );
    pprfs.push_back(PPRF(keys, points[i], outsize));
  }

  return pprfs;
}

////////////////////////////////////////////////////////////////////////////////
// DPF / D2PF SPECIFIC FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

std::vector<DPF> DPF::sample(size_t n, size_t keysize, size_t domainsize) {
  std::vector<DPF> dpfs;
  dpfs.reserve(n);
  for (size_t i = 0; i < n; i++) {
    dpfs.push_back(DPF(BitString::sample(keysize), domainsize));
  }
  return dpfs;
}

BitString DPF::image() const {
  BitString out(this->domainsize);
  for (uint32_t i = 0; i < this->domainsize; i++) {
    out[i] = this->operator()(i)[0];
  }
  return out;
}

void DPF::send(
    std::vector<DPF> dpfs, BitString payloads,
    std::shared_ptr<CommParty> channel, RandomOTSender rots
) {
  std::vector<std::pair<BitString, BitString>> messages;
  for (size_t i = 0; i < dpfs.size(); i++) {
    // for each level the messages are the left and right nodes xor'd
    for (size_t d = 1; d <= dpfs[i].depth; d++) {
      messages.push_back(dpfs[i].getLevelXORd(d));
    }

    // xor the final messages with the payload
    std::pair<BitString, BitString> leafs = dpfs[i].getLevelXORd(dpfs[i].depth);
    leafs.first[0] ^= payloads[i];
    leafs.second[0] ^= payloads[i];
    messages.push_back(leafs);
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

    // except the payload which should be the punctured leaf node
    choices += (!choices[choices.size() - 1]);

    // most of the ots are `keysize`-bit but the last two are bits for the leaf layer
    std::vector<size_t> branches(depth - 1, keysize);
    sizes.insert(sizes.end(), branches.begin(), branches.end());
    sizes.insert(sizes.end(), {1, 1});
  }

  // do the oblivious transfer
  std::vector<BitString> allkeys = rots.transfer(choices, sizes, channel);

  std::vector<DPF> dpfs;
  for (size_t i = 0; i < points.size(); i++) {
    std::vector<BitString> keys(
      allkeys.begin() + i * (depth + 1),
      allkeys.begin() + (i + 1) * (depth + 1)
    );
    dpfs.push_back(DPF(keys, points[i]));
  }

  return dpfs;
}

////////////////////////////////////////////////////////////////////////////////
// HELPER METHODS
////////////////////////////////////////////////////////////////////////////////

void PPRF::fill(size_t index) {
  // if `index` is a leaf node, nothing to fill
  if (index >= (1 << this->depth) - 1) { return; }

  // queue the nodes left to expand
  std::vector<size_t> queue({index});
  while (!queue.empty()) {
    size_t i = queue.back();
    queue.pop_back();

    BitString expanded = this->tree[i].aes(this->tree[i].size() * 2);
    this->tree[2 * i + 1] = expanded[{0, this->tree[i].size()}];
    this->tree[2 * i + 2] = expanded[{this->tree[i].size(), this->tree[i].size() * 2}];

    // shrink or expand leaf nodes as necessary
    if (isLeaf(2 * i + 1) && this->outsize < this->keysize) {
      this->tree[2 * i + 1] = this->tree[2 * i + 1][{0, outsize}];
      this->tree[2 * i + 2] = this->tree[2 * i + 2][{0, outsize}];
    } else if (isLeaf(2 * i + 1) && this->outsize > this->keysize) {
      this->tree[2 * i + 1] = this->tree[2 * i + 1].aes(outsize);
      this->tree[2 * i + 2] = this->tree[2 * i + 2].aes(outsize);
    }

    if (2 * i + 1 < (1 << this->depth) - 1) {
      queue.push_back(2 * i + 1);
      queue.push_back(2 * i + 2);
    }
  }
}

std::pair<BitString, BitString> PPRF::getLevelXORd(size_t l) const {
  if (l == 0 || l > this->depth) {
    throw std::domain_error("[PPRF::getLevel] incorrect level given");
  } else if (compressed) {
    throw std::logic_error("[PPRF::getLevel] cannot call on compressed tree");
  }

  BitString left(l == this->depth ? outsize : keysize);
  BitString right(l == this->depth ? outsize : keysize);
  for (size_t i = (1 << (l - 1)) - 1; i < (1 << l) - 1; i++) {
    if (this->tree[2 * i + 1].size() > 0) { left ^= this->tree[2 * i + 1]; }
    if (this->tree[2 * i + 2].size() > 0) { right ^= this->tree[2 * i + 2]; }
  }

  return std::make_pair(left, right);
}

BitString PPRF::getLeftXORd(size_t l) const {
  return this->getLevelXORd(l).first;
}

BitString PPRF::getRightXORd(size_t l) const {
  return this->getLevelXORd(l).second;
}
