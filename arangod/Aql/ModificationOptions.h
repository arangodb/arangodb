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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MODIFICATION_OPTIONS_H
#define ARANGOD_AQL_MODIFICATION_OPTIONS_H 1

#include "Basics/Common.h"
#include "Utils/OperationOptions.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

namespace aql {

/// @brief ModificationOptions
struct ModificationOptions : OperationOptions {
  /// @brief constructor, using default values
  explicit ModificationOptions(arangodb::velocypack::Slice const&);

  ModificationOptions() 
      : OperationOptions(),
        ignoreErrors(false),
        ignoreDocumentNotFound(false),
        readCompleteInput(true),
        consultAqlWriteFilter(false),
        exclusive(false) {}

  void toVelocyPack(arangodb::velocypack::Builder&) const;

  bool ignoreErrors;
  bool ignoreDocumentNotFound;
  bool readCompleteInput;
  bool consultAqlWriteFilter;
  bool exclusive;
};

}  // namespace aql
}  // namespace arangodb

#endif
