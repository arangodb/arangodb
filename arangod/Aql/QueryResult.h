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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_RESULT_H
#define ARANGOD_AQL_QUERY_RESULT_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

class TransactionContext;

namespace aql {

struct QueryResult {
  QueryResult& operator=(QueryResult const& other) = delete;
  QueryResult(QueryResult&& other) = default;

  QueryResult(int code, std::string const& details)
      : code(code),
        cached(false),
        details(details),
        warnings(nullptr),
        result(nullptr),
        profile(nullptr),
        context(nullptr) {}

  explicit QueryResult(int code) : QueryResult(code, "") {}

  QueryResult() : QueryResult(TRI_ERROR_NO_ERROR) {}

  virtual ~QueryResult() {}

  int code;
  bool cached;
  std::string details;
  std::unordered_set<std::string> bindParameters;
  std::vector<std::string> collectionNames;
  std::shared_ptr<arangodb::velocypack::Builder> warnings;
  std::shared_ptr<arangodb::velocypack::Builder> result;
  std::shared_ptr<arangodb::velocypack::Builder> stats;
  std::shared_ptr<arangodb::velocypack::Builder> profile;
  std::shared_ptr<arangodb::TransactionContext> context;
};
}
}

#endif
