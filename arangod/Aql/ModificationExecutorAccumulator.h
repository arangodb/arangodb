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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MODIFICATION_EXECUTOR_ACCUMULATOR_H
#define ARANGOD_AQL_MODIFICATION_EXECUTOR_ACCUMULATOR_H

#include "Basics/Common.h"
#include "Basics/debugging.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

// Hack-i-ty-hack
//
// This class is a wrapper around a VPackBuilder which makes sure
// that the accumulated object is an array, and we do not have to
// take care that the array is open in the builder (and not accidentally closed
// or not closed).
//
class ModificationExecutorAccumulator {
 public:
  ModificationExecutorAccumulator() { reset(); }

  VPackSlice closeAndGetContents() {
    _accumulator.close();
    return _accumulator.slice();
  }

  void add(VPackSlice const& doc) {
    TRI_ASSERT(_accumulator.isOpenArray());
    _accumulator.add(doc);
  }

  void reset() {
    _accumulator.clear();
    _accumulator.openArray();
  }

  size_t nrOfDocuments() const { return _accumulator.slice().length(); }

 private:
  VPackBuilder _accumulator;
};

}  // namespace aql
}  // namespace arangodb

#endif
