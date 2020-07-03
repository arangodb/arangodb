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

#ifndef ARANGOD_AQL_QUERY_RESULT_V8_H
#define ARANGOD_AQL_QUERY_RESULT_V8_H 1

#include "Aql/QueryResult.h"

#include <v8.h>

namespace arangodb {
namespace aql {

struct QueryResultV8 : public QueryResult {
  QueryResultV8(QueryResultV8 const& other) = delete;
  QueryResultV8& operator=(QueryResultV8 const& other) = delete;

  QueryResultV8(QueryResultV8&& other) = default;

  explicit QueryResultV8(QueryResult&& other)
      : QueryResult(std::move(other)) {}

  QueryResultV8() : QueryResult() {}
  
  explicit QueryResultV8(Result const& res) 
      : QueryResult(res) {}
  
  explicit QueryResultV8(Result&& res) 
      : QueryResult(std::move(res)) {}
  
  void reset(Result const& result) {
    QueryResult::reset(result);
    v8Data.Clear();
  }
  
  void reset(Result&& result) {
    QueryResult::reset(std::move(result));
    v8Data.Clear();
  }
  
  v8::Local<v8::Array> v8Data;
};
}  // namespace aql
}  // namespace arangodb

#endif
