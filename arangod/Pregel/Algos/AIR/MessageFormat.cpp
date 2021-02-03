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

#include "MessageFormat.h"

namespace arangodb::pregel::algos::accumulators {

// MessageFormat
MessageFormat::MessageFormat() = default;

void MessageFormat::unwrapValue(VPackSlice s, message_type& message) const {
  message.fromVelocyPack(s);
}

void MessageFormat::addValue(VPackBuilder& arrayBuilder,
                             message_type const& message) const {
  message.toVelocyPack(arrayBuilder);
}

}  // namespace arangodb::pregel::algos::accumulators
