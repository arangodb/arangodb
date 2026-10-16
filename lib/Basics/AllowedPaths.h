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

#include <algorithm>
#include <vector>
#include <filesystem>

namespace arangodb {
struct AllowedPaths {
  auto addPath(std::filesystem::path path) { _allowed.emplace_back(path); }

  auto empty() const noexcept { return _allowed.empty(); }

  auto isAllowed(std::filesystem::path path) const -> bool {
    // Check that first path is a prefix of the second
    auto is_prefix = [](std::filesystem::path const& fst,
                        std::filesystem::path const& snd) -> bool {
      auto [a, _] = std::mismatch(std::begin(fst), std::end(fst),
                                  std::begin(snd), std::end(snd));
      return (a == std::end(fst));
    };

    return std::any_of(
        std::begin(_allowed), std::end(_allowed),
        [path, is_prefix](std::filesystem::path const& container) {
          return is_prefix(container, path);
        });
  }

  template<typename Inspector>
  friend auto inline inspect(Inspector& f, AllowedPaths& p) {
    return f.object(p).fields(f.field("allow", p._allowed));
  }

 private:
  std::vector<std::filesystem::path> _allowed;
};

}  // namespace arangodb
