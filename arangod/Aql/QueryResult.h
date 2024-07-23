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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"

#include <velocypack/Builder.h>

#include <memory>
#include <unordered_set>
#include <vector>

namespace arangodb {
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

  QueryResult() : cached(false), allowDirtyReads(false) {}

  explicit QueryResult(Result const& res)
      : result(res), cached(false), allowDirtyReads(false) {}

  explicit QueryResult(Result&& res)
      : result(std::move(res)), cached(false), allowDirtyReads(false) {}

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
  ErrorCode errorNumber() const { return result.errorNumber(); }
  bool is(ErrorCode errorNumber) const { return result.is(errorNumber); }
  bool isNot(ErrorCode errorNumber) const { return result.isNot(errorNumber); }
  std::string_view errorMessage() const { return result.errorMessage(); }

  uint64_t memoryUsage() const noexcept {
    uint64_t value = 0;
    for (auto const& it : bindParameters) {
      value += it.size() + 16; /* 16 bytes as an arbitrary overhead */
    }
    for (auto const& it : collectionNames) {
      value += it.size() + 16; /* 16 bytes as an arbitrary overhead */
    }
    if (data != nullptr && data->buffer() != nullptr) {
      value +=
          data->buffer()->size() + 256 /* 256 bytes as an arbitrary overhead */;
    }
    if (extra != nullptr && extra->buffer() != nullptr) {
      value += extra->buffer()->size() +
               256 /* 256 bytes as an arbitrary overhead */;
    }
    if (context != nullptr) {
      value += 256; /* 256 bytes as an arbitrary overhead */
    }

    return value;
  }

  Result result;
  bool cached;
  bool allowDirtyReads;  // indicate that query was done with dirty reads,
                         // we need to preserve this here, since query results
                         // can live longer than their query objects and the
                         // transaction therein and we might still need the
                         // information to produce the outgoing HTTP header!
  std::unordered_set<std::string> bindParameters;
  std::vector<std::string> collectionNames;
  std::shared_ptr<arangodb::velocypack::Builder> data;
  std::shared_ptr<arangodb::velocypack::Builder> extra;
  std::shared_ptr<transaction::Context> context;
};
}  // namespace aql
}  // namespace arangodb
