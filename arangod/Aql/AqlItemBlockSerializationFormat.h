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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQLITEMBLOCK_SERIALIZATION_FORMAT_H
#define ARANGOD_AQL_AQLITEMBLOCK_SERIALIZATION_FORMAT_H

#include <type_traits>

namespace arangodb {
namespace aql {

/// @brief Defines the serialization format of AQL item blocks
enum class SerializationFormat {
  // Format used in 3.5 and early. Not containing shadow rows
  CLASSIC = 0,
  // Use a hidden register for shadow rows. In classic versions all entries would be off by one.
  SHADOWROWS = 1
};

using SerializationFormatType = std::underlying_type_t<SerializationFormat>;

}  // namespace aql
}  // namespace arangodb
#endif
