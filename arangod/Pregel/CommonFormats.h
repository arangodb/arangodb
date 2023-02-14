////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <map>

#include "Pregel/GraphStore/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/SenderMessage.h"
#include "Pregel/SenderMessageFormat.h"
#include "Pregel/VertexComputation.h"

#include "Inspection/VPack.h"

namespace arangodb::pregel {

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

// Label propagation
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

/// Value for Hyperlink-Induced Topic Search (HITS; also known as
/// hubs and authorities) according to the paper
/// J. Kleinberg, Authoritative sources in a hyperlinked environment,
/// Journal of the ACM. 46 (5): 604–632, 1999,
/// http://www.cs.cornell.edu/home/kleinber/auth.pdf.
struct HITSKleinbergValue {
  double nonNormalizedAuth;
  double nonNormalizedHub;
  double normalizedAuth;
  double normalizedHub;
};

struct DMIDValue {
  constexpr static float INVALID_DEGREE = -1;
  float weightedInDegree = INVALID_DEGREE;
  std::map<VertexID, float> membershipDegree;
  std::map<VertexID, float> disCol;
};

struct DMIDMessage {
  DMIDMessage() {}
  DMIDMessage(VertexID const& pid, float val) : senderId(pid), weight(val) {}

  DMIDMessage(VertexID const& sender, VertexID const& leader)
      : senderId(sender), leaderId(leader) {}

  VertexID senderId;
  VertexID leaderId;
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
  void addNode(VertexID const& pregelId);
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
  std::vector<VertexID> parents;
  uint64_t vertexID;
  uint64_t color;
};

using CollectionIdType = uint16_t;
using ColorType = uint16_t;
using PropagatedColor = uint16_t;
using VectorOfColors = std::vector<PropagatedColor>;

struct ColorPropagationValue {
  CollectionIdType equivalenceClass;
  std::vector<bool> colors;

  static CollectionIdType none() {
    return std::numeric_limits<CollectionIdType>::max();
  }

  [[nodiscard]] bool contains(ColorType color) const {
    // todo assert that color < numColors
    return colors[color];
  }

  void add(ColorType color) {
    // todo assert that color < numColors
    colors[color] = true;
  }

  [[nodiscard]] VectorOfColors getColors() const {
    VectorOfColors result;
    auto const size = colors.size();
    result.reserve(size);
    for (size_t color = 0; color < size; ++color) {
      if (contains(static_cast<ColorType>(color))) {
        result.push_back(static_cast<decltype(result)::value_type>(color));
      }
    }
    result.shrink_to_fit();
    return result;
  }
};

struct ColorPropagationMessageValue {
  CollectionIdType equivalenceClass = 0;
  std::vector<PropagatedColor> colors;
};

template<typename Inspector>
auto inspect(Inspector& f, ColorPropagationMessageValue& x) {
  return f.object(x).fields(
      f.field(Utils::equivalenceClass, x.equivalenceClass),
      f.field(Utils::colors, x.colors));
}

struct ColorPropagationUserParameters {
  uint64_t maxGss;
  uint16_t numColors;
  std::string inputColorsFieldName;
  std::string outputColorsFieldName;
  std::string equivalenceClassFieldName;
};

template<typename Inspector>
auto inspect(Inspector& f, ColorPropagationUserParameters& x) {
  return f.object(x).fields(
      f.field(Utils::maxGSS, x.maxGss), f.field(Utils::numColors, x.numColors),
      f.field(Utils::inputColorsFieldName, x.inputColorsFieldName),
      f.field(Utils::outputColorsFieldName, x.outputColorsFieldName),
      f.field(Utils::equivalenceClass, x.equivalenceClassFieldName));
}

struct ColorPropagationValueMessageFormat
    : public MessageFormat<ColorPropagationMessageValue> {
  ColorPropagationValueMessageFormat() = default;
  void unwrapValue(VPackSlice s,
                   ColorPropagationMessageValue& value) const override {
    value = deserialize<ColorPropagationMessageValue>(s);
  }
  void addValue(VPackBuilder& arrayBuilder,
                ColorPropagationMessageValue const& value) const override {
    serialize(arrayBuilder, value);
  }
};

}  // namespace arangodb::pregel
