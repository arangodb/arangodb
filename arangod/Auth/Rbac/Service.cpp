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
////////////////////////////////////////////////////////////////////////////////
#include "Service.h"

#include "Basics/voc-errors.h"

namespace arangodb::rbac {

auto Service::check(Token /*token*/,
                    std::span<ActionResource const> /*queries*/) noexcept
    -> Result {
  // The base implementation fails closed. The production ServiceImpl overrides
  // this to evaluate the batch against the RBAC backend; test mocks override it
  // with programmed answers.
  return {TRI_ERROR_NOT_IMPLEMENTED,
          "RBAC authorization service check is not yet implemented"};
}

}  // namespace arangodb::rbac
