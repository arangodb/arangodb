////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

// NOTE: this files exists primarily to include these algorithm specfic structs
// in the
// cpp files to do template specialization

#pragma once

#include <iterator>
#include <map>

#include "Pregel/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/GraphStore.h"
#include "Pregel/MessageFormat.h"

namespace arangodb {
namespace pregel {

// Speaker-listerner Label propagation
struct SLPAValue {
  // our own initialized id
  uint64_t nodeId = 0;
  // number of received communities
  uint64_t numCommunities = 0;
  /// Memory used to hold the labelId and the count
  // used for memorizing communities
  std::map<uint64_t, uint64_t> memory;
};

struct LPValue {
  /// The desired partition the vertex want to migrate to.
  uint64_t currentCommunity = 0;
  /// The actual partition.
  uint64_t lastCommunity = UINT64_MAX;
  /// Iterations since last migration.
  uint64_t stabilizationRounds = 0;
};

/// Value for Hyperlink-Induced Topic Search (HITS; also known as
/// hubs and authorities)
struct HITSValue {
  double authorityScore;
  double hubScore;
};

struct DMIDValue {
  constexpr static float INVALID_DEGREE = -1;
  float weightedInDegree = INVALID_DEGREE;
  std::map<PregelID, float> membershipDegree;
  std::map<PregelID, float> disCol;
};

struct DMIDMessage {
  DMIDMessage() {}
  DMIDMessage(PregelID const& pid, float val) : senderId(pid), weight(val) {}

  DMIDMessage(PregelID const& sender, PregelID const& leader)
      : senderId(sender), leaderId(leader) {}

  PregelID senderId;
  PregelID leaderId;
  float weight = 0;
};

/// A counter for counting unique vertex IDs using a HyperLogLog sketch.
/// @author Aljoscha Krettek, Robert Metzger, Robert Waury
/// https://github.com/hideo55/cpp-HyperLogLog/blob/master/include/hyperloglog.hpp
/// https://github.com/rmetzger/spargel-closeness/blob/master/src/main/java/de/robertmetzger/HLLCounterWritable.java
struct HLLCounter {
  friend struct HLLCounterFormat;
  constexpr static int32_t NUM_BUCKETS = 64;
  constexpr static double ALPHA = 0.709;

  uint32_t getCount();
  void addNode(PregelID const& pregelId);
  void merge(HLLCounter const& counter);

 private:
  uint8_t _buckets[NUM_BUCKETS] = {0};
};

/// Effective closeness value
struct ECValue {
  HLLCounter counter;
  std::vector<uint32_t> shortestPaths;
};

struct SCCValue {
  std::vector<PregelID> parents;
  uint64_t vertexID;
  uint64_t color;
};

struct WCCValue {
  struct InNeighbor {
    PregelShard fromShard;
    uint16_t keylen;
    char fromKey[];

    auto toPregelID() const -> PregelID {
      return PregelID(fromShard, std::string(fromKey, keylen));
    }
    auto size() const -> size_t { return sizeof(InNeighbor) + keylen; }
  };
  struct InNeighbors {
    struct Iterator {
      using iterator_category = std::input_iterator_tag;
      using difference_type = std::ptrdiff_t;
      using value_type = PregelID;
      using pointer = PregelID*;
      using reference = PregelID&;

      Iterator(MemRange range) : range(range) {
        if (range.from < range.to) {
          id = reinterpret_cast<InNeighbor*>(range.from)->toPregelID();
          ADB_PROD_ASSERT(id.isValid());
        }
      }
      Iterator(Iterator const& other) : range(other.range), id(other.id) {}

      reference operator*() const { return const_cast<reference>(id); }
      pointer operator->() { return &id; }
      Iterator& operator++() {
        range.from += reinterpret_cast<InNeighbor*>(range.from)->size();
        if (range.from < range.to) {
          id = reinterpret_cast<InNeighbor*>(range.from)->toPregelID();
          ADB_PROD_ASSERT(id.isValid());
        }
        return *this;
      }
      Iterator operator++(int) {
        Iterator tmp = *this;
        ++(*this);
        return tmp;
      }
      friend bool operator==(const Iterator& l, const Iterator& r) {
        return l.range.from == r.range.from;
      }
      friend bool operator!=(const Iterator& l, const Iterator& r) {
        return l.range.from != r.range.from;
      }

      //     private:
      MemRange range;
      PregelID id;
    };

    MemRange storage = MemRange{.from = nullptr, .to = nullptr};
    // FIXME: output iterator?
    uint8_t* current = nullptr;
    uint32_t count = 0;

    auto init(MemRange inStorage, uint32_t inCount) -> void {
      storage = inStorage;
      current = const_cast<uint8_t*>(storage.from);
      count = inCount;
    }

    auto emplace(PregelID pid) -> void {
      auto storedInNeighbor = reinterpret_cast<InNeighbor*>(current);
      storedInNeighbor->fromShard = pid.shard;
      storedInNeighbor->keylen = pid.key.length();
      memmove(storedInNeighbor->fromKey, pid.key.c_str(),
              storedInNeighbor->keylen);

      current += storedInNeighbor->size();
    }

    auto begin() const -> Iterator { return Iterator(storage); }
    auto end() const -> Iterator {
      return Iterator(MemRange{.from = storage.to, .to = storage.to});
    }
  };

  uint64_t component;
  InNeighbors inNeighbors;
};

template<typename T>
struct SenderMessage {
  SenderMessage() = default;
  SenderMessage(PregelID pid, T const& val)
      : senderId(std::move(pid)), value(val) {}

  PregelID senderId;
  T value;
};

template<typename T>
struct SenderMessageFormat : public MessageFormat<SenderMessage<T>> {
  static_assert(std::is_arithmetic<T>::value, "Message type must be numeric");
  SenderMessageFormat() {}
  void unwrapValue(VPackSlice s, SenderMessage<T>& senderVal) const override {
    VPackArrayIterator array(s);
    senderVal.senderId.shard = (PregelShard)((*array).getUInt());
    senderVal.senderId.key = (*(++array)).copyString();
    senderVal.value = (*(++array)).getNumber<T>();
  }
  void addValue(VPackBuilder& arrayBuilder,
                SenderMessage<T> const& senderVal) const override {
    arrayBuilder.openArray();
    arrayBuilder.add(VPackValue(senderVal.senderId.shard));
    arrayBuilder.add(VPackValuePair(senderVal.senderId.key.data(),
                                    senderVal.senderId.key.size(),
                                    VPackValueType::String));
    arrayBuilder.add(VPackValue(senderVal.value));
    arrayBuilder.close();
  }
};
}  // namespace pregel
}  // namespace arangodb
