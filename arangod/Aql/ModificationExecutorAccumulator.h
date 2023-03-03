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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb::aql {

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

  [[nodiscard]] VPackSlice closeAndGetContents() {
    TRI_ASSERT(_accumulator.isOpenArray());
    _accumulator.close();
    TRI_ASSERT(_accumulator.isClosed());
    return _accumulator.slice();
  }

  void add(VPackSlice doc) {
    TRI_ASSERT(_accumulator.isOpenArray());
    _accumulator.add(doc);
    ++_nrOfDocuments;
  }

  void reset() {
    _accumulator.clear();
    _accumulator.openArray(/*unindexed*/ true);
    _nrOfDocuments = 0;
  }

  [[nodiscard]] size_t nrOfDocuments() const noexcept {
    TRI_ASSERT(_accumulator.isClosed());
    return _nrOfDocuments;
  }

 private:
  VPackBuilder _accumulator{};
  // storing the number of documents separately allows us
  // to return the value without having to call slice().length()
  // on the Builder. it also allows us to use a compact velocypack
  // array inside the Builder without an index table (smaller
  // size, less overhead when closing the array).
  size_t _nrOfDocuments{0};
};

}  // namespace arangodb::aql
