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

#pragma once

#include "Auth/Rbac/Actions.h"
#include "Basics/Result.h"

#include <span>

namespace arangodb::rbac {

struct Service {
  virtual ~Service() = default;

  // Ask a batch of authorization questions for a single subject at once. A
  // real implementation performs a single network round-trip, so callers that
  // need several (action, resource) pairs for one logical permission should
  // pass them together rather than calling check() repeatedly. Returns ok iff
  // every pair is permitted; otherwise the first/aggregated denial.
  //
  // Virtual so that tests (in particular the RBAC auth-mode tests) can inject a
  // mock Service. The base implementation fails closed; see Service.cpp.
  virtual auto check(Subject const& subject,
                     std::span<ActionResource const> queries) -> Result;
};

}  // namespace arangodb::rbac
