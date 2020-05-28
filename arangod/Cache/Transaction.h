////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_TRANSACTION_H
#define ARANGODB_CACHE_TRANSACTION_H

#include <cstdint>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Structure to maintain information about an individual transaction.
struct Transaction {
  std::uint64_t term;
  bool readOnly;
  bool sensitive;

  Transaction();
  explicit Transaction(bool ro);
};

};  // end namespace cache
};  // end namespace arangodb

#endif
