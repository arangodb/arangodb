////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>

#ifndef ARANGODB_PREGEL_COMBINER_H
#define ARANGODB_PREGEL_COMBINER_H 1
namespace arangodb {
namespace pregel {

// specify serialization, whatever
template <class M>
struct MessageCombiner {
  virtual ~MessageCombiner() = default;
  virtual void combine(M& firstValue, M const& secondValue) const = 0;
};

template <typename M>
struct MinCombiner : public MessageCombiner<M> {
  static_assert(std::is_arithmetic<M>::value, "Message type must be numeric");
  MinCombiner() {}
  void combine(M& firstValue, M const& secondValue) const override {
    if (firstValue > secondValue) {
      firstValue = secondValue;
    }
  };
};

template <typename M>
struct SumCombiner : public MessageCombiner<M> {
  static_assert(std::is_arithmetic<M>::value, "Message type must be numeric");
  SumCombiner() {}
  void combine(M& firstValue, M const& secondValue) const override {
    firstValue += secondValue;
  }
};
}  // namespace pregel
}  // namespace arangodb
#endif
