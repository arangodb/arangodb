////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Query.h"

namespace arangodb::tests::mocks {

struct MockQuery final : aql::Query {
  MockQuery(std::shared_ptr<transaction::Context> const& ctx,
            aql::QueryString const& queryString)
      : aql::Query{ctx, queryString, nullptr,
                   {},  nullptr,     transaction::TrxType::kInternal} {}

  ~MockQuery() final {
    // Destroy this query, otherwise it's still
    // accessible while the query is being destructed,
    // which can result in a data race on the vptr
    destroy();
  }

  transaction::Methods& trxForOptimization() final {
    // original version contains an assertion
    return *_trx;
  }
};
}  // namespace arangodb::tests::mocks
