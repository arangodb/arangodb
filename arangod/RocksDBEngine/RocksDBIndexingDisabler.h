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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBCommon.h"

namespace arangodb {
class RocksDBMethods;

// INDEXING MAY ONLY BE DISABLED IN TOPLEVEL AQL TRANSACTIONS
// THIS IS BECAUSE THESE TRANSACTIONS WILL EITHER READ FROM
// OR (XOR) WRITE TO A COLLECTION. IF THIS PRECONDITION IS
// VIOLATED THE DISABLED INDEXING WILL BREAK GET OPERATIONS.
struct IndexingDisabler {
  IndexingDisabler() = delete;
  IndexingDisabler(IndexingDisabler&&) = delete;
  IndexingDisabler(IndexingDisabler const&) = delete;
  IndexingDisabler& operator=(IndexingDisabler const&) = delete;
  IndexingDisabler& operator=(IndexingDisabler&&) = delete;

  // will only be active if condition is true
  IndexingDisabler(RocksDBMethods* methods, bool condition);
  ~IndexingDisabler();

 private:
  RocksDBMethods* _methods;
};

// if only single indices should be enabled during operations
struct IndexingEnabler {
  IndexingEnabler() = delete;
  IndexingEnabler(IndexingEnabler&&) = delete;
  IndexingEnabler(IndexingEnabler const&) = delete;
  IndexingEnabler& operator=(IndexingEnabler const&) = delete;
  IndexingEnabler& operator=(IndexingEnabler&&) = delete;

  // will only be active if condition is true
  IndexingEnabler(RocksDBMethods* methods, bool condition);
  ~IndexingEnabler();

 private:
  RocksDBMethods* _methods;
};

}  // namespace arangodb
