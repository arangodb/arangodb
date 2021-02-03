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

#include "Basics/debugging.h"
#include "MessageData.h"

#include "velocypack/velocypack-aliases.h"

using namespace arangodb::pregel::algos::accumulators;

void MessageData::reset(std::string accumulatorName, VPackSlice const& value, std::string const& sender) {
  _accumulatorName = accumulatorName;
  _sender = sender;
  _value.clear();
  _value.add(value);
}

void MessageData::fromVelocyPack(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());

  // TODO: UUUUGLLLAAAAYYy
  _accumulatorName = slice.get("accumulatorName").copyString();
  _sender = slice.get("sender").copyString();
  _value.clear();
  _value.add(slice.get("value"));
}

void MessageData::toVelocyPack(VPackBuilder& builder) const {
  auto guard = VPackObjectBuilder{&builder};

  builder.add(VPackValue("accumulatorName"));
  builder.add(VPackValue(accumulatorName()));
  builder.add(VPackValue("sender"));
  builder.add(VPackValue(sender()));
  builder.add(VPackValue("value"));
  builder.add(value().slice());
}

auto MessageData::accumulatorName() const -> std::string const& {
  return _accumulatorName;
}
auto MessageData::sender() const -> std::string const& {
  return _sender;
}
auto MessageData::value() const -> VPackBuilder const& {
  return _value;
}
