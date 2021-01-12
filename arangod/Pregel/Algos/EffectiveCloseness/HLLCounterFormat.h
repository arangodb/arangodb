////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_ALGOS_EC_HLLCounter_H
#define ARANGODB_PREGEL_ALGOS_EC_HLLCounter_H 1

#include "Pregel/CommonFormats.h"
#include "Pregel/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"

namespace arangodb {
namespace pregel {

struct HLLCounterFormat : public MessageFormat<HLLCounter> {
  HLLCounterFormat() {}
  void unwrapValue(VPackSlice s, HLLCounter& senderVal) const override {
    VPackArrayIterator array(s);
    for (size_t i = 0; i < HLLCounter::NUM_BUCKETS; i++) {
      senderVal._buckets[i] = (uint8_t)(*array).getUInt();
      ++array;
    }
  }
  void addValue(VPackBuilder& arrayBuilder, HLLCounter const& senderVal) const override {
    // TODO fucking pack 8-bytes into one 64 bit entry
    arrayBuilder.openArray();
    for (size_t i = 0; i < HLLCounter::NUM_BUCKETS; i++) {
      arrayBuilder.add(VPackValue(senderVal._buckets[i]));
    }
    arrayBuilder.close();
  }
};

struct HLLCounterCombiner : MessageCombiner<HLLCounter> {
  void combine(HLLCounter& firstValue, HLLCounter const& secondValue) const override {
    firstValue.merge(secondValue);
  };
};
}  // namespace pregel
}  // namespace arangodb

#endif
