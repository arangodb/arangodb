////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Transaction/MethodsApi.h"

namespace arangodb::rbac {

struct Backend {
  virtual ~Backend() = default;

  // types
  enum class Effect { Allow, Deny };
  struct ResponseItem {
    Effect effect;
    std::string message;
  };
  using EvaluateResponse = ResponseItem;
  struct EvaluateResponseMany : ResponseItem {
    std::vector<ResponseItem> items;
  };

  struct RequestItem {
    // .action
    std::string action;
    // .resource
    std::string resource;
    // .context.parameters.attribute.values:
    std::vector<std::string> attributeValues;
  };
  struct RequestItems {
    std::vector<RequestItem> items;
  };
  struct PlainUser {
    std::string username;
    std::vector<std::string> roles;
  };
  struct JwtToken {
    std::string jwtToken;
  };

  // functions

  // TODO We need to extend the API to handle callers that will wait
  //      synchronously on the response; i.a. so that `skipScheduler: true` can
  //      be set. Figure out how best to do that (e.g., a parameter, or
  //      additional functions?).

  // public API, asynchronous versions

  // Batched APIs
  auto evaluateTokenMany(JwtToken const&, RequestItems const&)
      -> futures::Future<ResultT<EvaluateResponseMany>>;
  auto evaluateMany(PlainUser const&, RequestItems const&)
      -> futures::Future<ResultT<EvaluateResponseMany>>;

  // Single-item APIs

  // Note that for simplicity, the single-item APIs aren't explicitly
  // implemented, but instead fall back to using the batched APIs with a single
  // item.
  // For that reason, there are no tests for these methods in
  // RbacBackendTest.cpp, nor in RbacIntegrationTest.cpp: They wouldn't do the
  // expected API calls to the single-APIs, but call the batch-APIs instead.
  // Additionally, they aren't used by the ServiceImpl.
  // TODO We might want to make a decision whether to delete them, or implement
  //      and test them properly; keeping unused and untested code isn't a good
  //      idea, even if it means we don't implement the full API surface of the
  //      authorization service.
  auto evaluateToken(JwtToken const& jwtToken, RequestItem const& item)
      -> futures::Future<ResultT<EvaluateResponse>>;
  auto evaluate(PlainUser const& user, RequestItem const& item)
      -> futures::Future<ResultT<EvaluateResponse>>;

  // public API, synchronous versions
  [[deprecated("Use the asynchronous counterpart instead")]] auto
  evaluateTokenManySync(JwtToken const&, RequestItems const&)
      -> ResultT<EvaluateResponseMany>;
  [[deprecated("Use the asynchronous counterpart instead")]] auto
  evaluateManySync(PlainUser const&, RequestItems const&)
      -> ResultT<EvaluateResponseMany>;
  [[deprecated("Use the asynchronous counterpart instead")]] auto
  evaluateTokenSync(JwtToken const& jwtToken, RequestItem const& item)
      -> ResultT<EvaluateResponse>;
  [[deprecated("Use the asynchronous counterpart instead")]] auto evaluateSync(
      PlainUser const& user, RequestItem const& item)
      -> ResultT<EvaluateResponse>;

 protected:
  // API implementation
  // Distinguishing between non-virtual public and non-public virtual
  // api methods is (only) necessary to provide both synchronous and
  // asynchronous variants.

  // essential functions
  virtual auto evaluateTokenManyImpl(JwtToken const&, RequestItems const&,
                                     transaction::MethodsApi api)
      -> futures::Future<ResultT<EvaluateResponseMany>> = 0;
  virtual auto evaluateManyImpl(PlainUser const&, RequestItems const&,
                                transaction::MethodsApi api)
      -> futures::Future<ResultT<EvaluateResponseMany>> = 0;

  // unessential functions with fallback implementations
  // (there are corresponding APIs, but they don't provide additional
  // functionality)
  virtual auto evaluateTokenImpl(JwtToken const& jwtToken,
                                 RequestItem const& item,
                                 transaction::MethodsApi api)
      -> futures::Future<ResultT<EvaluateResponse>> {
    auto result = co_await evaluateTokenManyImpl(
        jwtToken, RequestItems{.items = {item}}, api);
    co_return result.map([](EvaluateResponseMany const& r) -> EvaluateResponse {
      return r;  // NOLINT(cppcoreguidelines-slicing)
    });
  };
  virtual auto evaluateImpl(PlainUser const& user, RequestItem const& item,
                            transaction::MethodsApi api)
      -> futures::Future<ResultT<EvaluateResponse>> {
    auto result =
        co_await evaluateManyImpl(user, RequestItems{.items = {item}}, api);
    co_return result.map([](EvaluateResponseMany const& r) -> EvaluateResponse {
      return r;  // NOLINT(cppcoreguidelines-slicing)
    });
  };
};

}  // namespace arangodb::rbac
