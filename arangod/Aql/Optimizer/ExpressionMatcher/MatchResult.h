////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <optional>

#include <Aql/AstNode.h>

namespace arangodb::aql::expression_matcher {
struct MatchResult {
  auto isSuccess() const noexcept -> bool { return _errors.empty(); };
  auto isError() const noexcept -> bool { return not _errors.empty(); }

  struct ErrorTag {};
  struct ValueTag {};

  explicit MatchResult() = default;
  MatchResult(MatchResult&&) = default;
  MatchResult(MatchResult const&) = delete;

  auto addError(std::string error) && -> MatchResult&& {
    _errors.emplace_back(error);
    return std::move(*this);
  }

  auto addErrorIfError(std::string error) && -> MatchResult&& {
    if (isError()) {
      _errors.emplace_back(error);
    }
    return std::move(*this);
  }

  auto addMatch(std::string name, AstNode const* value) && -> MatchResult&& {
    _matches.emplace(name, value);
    return std::move(*this);
  }

  auto combine(MatchResult&& rhs) && -> MatchResult&& {
    _errors.insert(std::end(_errors), std::begin(rhs._errors),
                   std::end(rhs._errors));
    _matches.merge(rhs._matches);

    return std::move(*this);
  }

  static auto error(std::string message) -> MatchResult {
    return MatchResult().addError(message);
  }
  static auto match(std::string key, AstNode const* value) {
    return MatchResult().addMatch(key, value);
  }
  static auto match() { return MatchResult(); }

  auto errors() const noexcept -> std::vector<std::string> const& {
    return _errors;
  }
  auto matches() const noexcept
      -> std::unordered_map<std::string, AstNode const*> const& {
    return _matches;
  }

  friend inline auto operator/(MatchResult&& lhs, std::string error)
      -> MatchResult&& {
    lhs._errors.emplace_back(error);
    return std::move(lhs);
  }

  template<typename Func>
  friend inline auto operator|(MatchResult&& lhs, Func&& func)
      -> MatchResult&& {
    if (lhs.isError()) {
      return std::move(lhs);
    }
    return std::move(lhs).combine(func());
  }

 private:
  std::unordered_map<std::string, AstNode const*> _matches;
  std::vector<std::string> _errors;
};

auto toStream(std::ostream&, MatchResult const& result) -> std::ostream&;
}  // namespace arangodb::aql::expression_matcher
