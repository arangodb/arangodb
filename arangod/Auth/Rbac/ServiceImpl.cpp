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

ServiceImpl::ServiceImpl(std::unique_ptr<Backend> backend)
    : _backend(std::move(backend)) {}

auto ServiceImpl::mayImpl(User user, std::string action,
                          std::string resource) noexcept
    -> async<ResultT<bool>> {
  auto result = co_await _backend->evaluateToken(
      Backend::JwtToken{.jwtToken = std::move(user.jwtToken)},
      Backend::RequestItem{.action = std::move(action),
                           .resource = std::move(resource)});
  if (!result.ok()) {
    co_return result.result();
  }
  co_return result.get().effect == Backend::Effect::Allow;
}

auto ServiceImpl::maySyncImpl(User user, std::string action,
                              std::string resource) noexcept -> ResultT<bool> {
  auto result = _backend->evaluateTokenSync(
      Backend::JwtToken{.jwtToken = std::move(user.jwtToken)},
      Backend::RequestItem{.action = std::move(action),
                           .resource = std::move(resource)});
  if (!result.ok()) {
    return result.result();
  }
  return result.get().effect == Backend::Effect::Allow;
}

}  // namespace arangodb::rbac
