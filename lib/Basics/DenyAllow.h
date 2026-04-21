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
#include <string>
#include <optional>
#include <regex>

namespace arangodb {
enum class DenyAllowResult { DENIED, ALLOWED };
struct DenyAllow {
  DenyAllow() = default;
  explicit DenyAllow(std::optional<std::regex> deny,
                     std::optional<std::regex> allow)
      : _deny(std::move(deny)), _allow(std::move(allow)) {}
  explicit DenyAllow(std::string denyList, std::string allowList) {
    auto constexpr regex_opts = std::regex::nosubs | std::regex::ECMAScript;
    if (!denyList.empty()) {
      _deny = std::regex(denyList, regex_opts);
    }
    if (!allowList.empty()) {
      _allow = std::regex(allowList, regex_opts);
    }
  }

  // TODO: I am serverely uneasy with the use of regular expressions
  // for allowlists for paths, startup options, environment variables,
  // COR-349 asks the question whether regular expressions are the correct
  // tool for the job and if so to refactor this code (and if not to replace
  // it).
  auto check(std::string v) const -> DenyAllowResult {
    if (_deny.has_value()) {
      if (std::regex_search(v, _deny.value())) {
        return DenyAllowResult::DENIED;
      }
    }
    if (_allow.has_value()) {
      if (std::regex_search(v, _allow.value())) {
        return DenyAllowResult::ALLOWED;
      }
    }
    return DenyAllowResult::DENIED;
  }

 private:
  std::optional<std::regex> _deny{std::nullopt};
  std::optional<std::regex> _allow{std::nullopt};
};

}  // namespace arangodb
