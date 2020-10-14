////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQLVALUEMATERIALIZER_H
#define ARANGOD_AQL_AQLVALUEMATERIALIZER_H

#include "Aql/AqlValue.h"

namespace arangodb {

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
struct Options;
}  // namespace velocypack

namespace aql {

/// @brief Helper class to materialize AqlValues (see AqlValue::materialize).
struct AqlValueMaterializer {
  explicit AqlValueMaterializer(velocypack::Options const* options);

  AqlValueMaterializer(AqlValueMaterializer const& other);

  // cppcheck-suppress operatorEqVarError
  AqlValueMaterializer& operator=(AqlValueMaterializer const& other);

  AqlValueMaterializer(AqlValueMaterializer&& other) noexcept;

  // cppcheck-suppress operatorEqVarError
  AqlValueMaterializer& operator=(AqlValueMaterializer&& other) noexcept;

  ~AqlValueMaterializer();

  arangodb::velocypack::Slice slice(arangodb::aql::AqlValue const& value, bool resolveExternals);

  arangodb::velocypack::Options const* options;
  arangodb::aql::AqlValue materialized;
  bool hasCopied;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_AQLVALUEMATERIALIZER_H
