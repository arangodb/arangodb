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

#include "ServiceImpl.h"

namespace arangodb::rbac {

namespace {

auto flattenQueries(std::vector<Service::AuthorizationQuery> const& queries)
    -> Backend::RequestItems {
  Backend::RequestItems items;
  for (auto const& q : queries) {
    items.items.push_back(Backend::RequestItem{
        .action = q.action, .resource = q.resource, .attributeValues = {}});
  }
  return items;
}

}  // namespace

ServiceImpl::ServiceImpl(std::unique_ptr<Backend> backend)
    : _backend(std::move(backend)) {}

auto ServiceImpl::mayImpl(User user,
                          std::vector<AuthorizationQuery> queries) noexcept
    -> async<ResultT<bool>> {
  auto items = flattenQueries(queries);
  auto result = co_await _backend->evaluateTokenMany(
      Backend::JwtToken{.jwtToken = std::move(user.jwtToken)}, items);
  if (!result.ok()) {
    co_return result.result();
  }
  co_return result.get().effect == Backend::Effect::Allow;
}

auto ServiceImpl::maySyncImpl(User user,
                              std::vector<AuthorizationQuery> queries) noexcept
    -> ResultT<bool> {
  auto items = flattenQueries(queries);
  auto result = _backend->evaluateTokenManySync(
      Backend::JwtToken{.jwtToken = std::move(user.jwtToken)}, items);
  if (!result.ok()) {
    return result.result();
  }
  return result.get().effect == Backend::Effect::Allow;
}

}  // namespace arangodb::rbac
