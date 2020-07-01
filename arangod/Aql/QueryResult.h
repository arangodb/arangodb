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

#include <memory>
#include <unordered_set>
#include <vector>

#include "Basics/Common.h"
#include "Basics/Result.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace transaction {
class Context;
}

namespace aql {

struct QueryResult {
  // no copying, but moving is allowed
  QueryResult(QueryResult const& other) = delete;
  QueryResult& operator=(QueryResult const& other) = delete;
  QueryResult(QueryResult&& other) = default;
  QueryResult& operator=(QueryResult&& other) = default;

  QueryResult()
      : result(),
        cached(false) {}

  explicit QueryResult(Result const& res)
      : result(res),
        cached(false) {}

  explicit QueryResult(Result&& res)
      : result(std::move(res)),
        cached(false) {}

  virtual ~QueryResult() = default;

  void reset(Result const& res) {
    result.reset(res);
    cached = false;
    data.reset();
    extra.reset();
    context.reset();
  }

  void reset(Result&& res) {
    result.reset(std::move(res));
    cached = false;
    data.reset();
    extra.reset();
    context.reset();
  }

  // Result-like interface
  bool ok() const { return result.ok(); }
  bool fail() const { return result.fail(); }
  int errorNumber() const { return result.errorNumber(); }
  bool is(int errorNumber) const { return result.errorNumber() == errorNumber; }
  bool isNot(int errorNumber) const { return !is(errorNumber); }
  std::string errorMessage() const { return result.errorMessage(); }

 public:
  Result result;
  bool cached;
  std::unordered_set<std::string> bindParameters;
  std::vector<std::string> collectionNames;
  std::shared_ptr<arangodb::velocypack::Builder> data;
  std::shared_ptr<arangodb::velocypack::Builder> extra;
  std::shared_ptr<transaction::Context> context;
};
}  // namespace aql
}  // namespace arangodb

#endif
