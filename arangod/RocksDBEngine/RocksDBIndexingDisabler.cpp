////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBIndexingDisabler.h"

#include "RocksDBEngine/RocksDBMethods.h"

namespace arangodb {
class RocksDBMethods;

// will only be active if condition is true
IndexingDisabler::IndexingDisabler(RocksDBMethods* methods, bool condition)
    : _methods(nullptr) {
  if (condition) {
    bool disabledHere = methods->DisableIndexing();
    if (disabledHere) {
      _methods = methods;
    }
  }
}

IndexingDisabler::~IndexingDisabler() {
  if (_methods) {
    _methods->EnableIndexing();
  }
}

// will only be active if condition is true
IndexingEnabler::IndexingEnabler(RocksDBMethods* methods, bool condition)
    : _methods(nullptr) {
  if (condition) {
    bool enabledHere = methods->EnableIndexing();
    if (enabledHere) {
      _methods = methods;
    }
  }
}

IndexingEnabler::~IndexingEnabler() {
  if (_methods) {
    _methods->DisableIndexing();
  }
}

}  // namespace arangodb
