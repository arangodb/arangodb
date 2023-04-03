////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Inspection/Access.h"
#include <string>

namespace arangodb {

namespace inspection {
struct Status;
}  // namespace inspection

struct UtilityTransformers {
  struct EmptyStringEqualsUnset {
    using MemoryType = std::string;
    using SerializedType = std::string;

    static arangodb::inspection::Status toSerialized(MemoryType const& v,
                                                     SerializedType& result);
    static arangodb::inspection::Status fromSerialized(
        SerializedType& v, inspection::NonNullOptional<std::string>& result);
  };
};

}  // namespace arangodb
