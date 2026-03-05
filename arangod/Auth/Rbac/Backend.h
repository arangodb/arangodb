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

  // essential functions
  virtual auto evaluateTokenMany(JwtToken const&, RequestItems const&)
      -> futures::Future<ResultT<EvaluateResponseMany>> = 0;
  virtual auto evaluateMany(PlainUser const&, RequestItems const&)
      -> futures::Future<ResultT<EvaluateResponseMany>> = 0;

  // unessential functions with fallback implementations
  // (there are corresponding APIs, but they don't provide additional
  // functionality)
  virtual auto evaluateToken(JwtToken const& jwtToken, RequestItem const& item)
      -> futures::Future<ResultT<EvaluateResponse>> {
    auto result =
        co_await evaluateTokenMany(jwtToken, RequestItems{.items = {item}});
    co_return result.map([](EvaluateResponseMany const& r) -> EvaluateResponse {
      return r;  // NOLINT(cppcoreguidelines-slicing)
    });
  };
  virtual auto evaluate(PlainUser const& user, RequestItem const& item)
      -> futures::Future<ResultT<EvaluateResponse>> {
    auto result = co_await evaluateMany(user, RequestItems{.items = {item}});
    co_return result.map([](EvaluateResponseMany const& r) -> EvaluateResponse {
      return r;  // NOLINT(cppcoreguidelines-slicing)
    });
  };
};

}  // namespace arangodb::rbac
