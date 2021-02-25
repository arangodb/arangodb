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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_MESSAGEDATA_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_MESSAGEDATA_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <string>

using namespace arangodb::velocypack;

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

struct MessageData {
  void reset(std::string accumulatorName, VPackSlice const& value, std::string const& sender);

  void fromVelocyPack(VPackSlice slice);
  void toVelocyPack(VPackBuilder& b) const;

  auto accumulatorName() const -> std::string const&;
  auto sender() const -> std::string const&;
  auto value() const -> VPackBuilder const&;

  std::string _accumulatorName;

  // We copy the value :/ is this necessary?
  VPackBuilder _value;
  std::string _sender;
};
}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb

#endif
