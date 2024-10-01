////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Utils/OperationOptions.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {

struct ModificationOptions : OperationOptions {
  // constructor, using velocypack input
  explicit ModificationOptions(velocypack::Slice slice);

  // constructor, using default values
  ModificationOptions()
      : OperationOptions(),
        ignoreErrors(false),
        ignoreDocumentNotFound(false),
        consultAqlWriteFilter(false),
        exclusive(false) {}

  void toVelocyPack(velocypack::Builder&) const;

  bool ignoreErrors;
  bool ignoreDocumentNotFound;
  bool consultAqlWriteFilter;
  bool exclusive;
};

}  // namespace aql
}  // namespace arangodb
