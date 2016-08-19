////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {

/// @brief ModificationOptions
struct ModificationOptions {
  /// @brief constructor, using default values
  explicit ModificationOptions(arangodb::velocypack::Slice const&);

  ModificationOptions()
      : ignoreErrors(false),
        waitForSync(false),
        nullMeansRemove(false),
        mergeObjects(true),
        ignoreDocumentNotFound(false),
        readCompleteInput(true) {}

  void toVelocyPack(arangodb::velocypack::Builder&) const;

  bool ignoreErrors;
  bool waitForSync;
  bool nullMeansRemove;
  bool mergeObjects;
  bool ignoreDocumentNotFound;
  bool readCompleteInput;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
