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

#include "Basics/Result.h"

#include <memory>
#include <string_view>

struct TRI_vocbase_t;

namespace arangodb::aql {
class QueryContext;

class StandaloneCalculation {
 public:
  static std::unique_ptr<QueryContext> buildQueryContext(
      TRI_vocbase_t& vocbase);

  static arangodb::Result validateQuery(TRI_vocbase_t& vocbase,
                                        std::string_view queryString,
                                        std::string_view parameterName,
                                        char const* errorContext);
};

}  // namespace arangodb::aql
