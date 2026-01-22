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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <atomic>
#include <cstdint>
#include <source_location>
#include <string_view>

namespace arangodb::basics {

struct SourceLocationSnapshot {
  std::string_view file_name;
  std::string_view function_name;
  std::uint_least32_t line;
  bool operator==(SourceLocationSnapshot const&) const = default;
  static auto from(std::source_location loc) -> SourceLocationSnapshot {
    return SourceLocationSnapshot{.file_name = loc.file_name(),
                                  .function_name = loc.function_name(),
                                  .line = loc.line()};
  }
};
template<typename Inspector>
auto inspect(Inspector& f, SourceLocationSnapshot& x) {
  return f.object(x).fields(f.field("file_name", x.file_name),
                            f.field("line", x.line),
                            f.field("function_name", x.function_name));
}

struct VariableSourceLocation {
  auto snapshot() -> SourceLocationSnapshot {
    return SourceLocationSnapshot{.file_name = file_name,
                                  .function_name = function_name,
                                  .line = line.load()};
  }
  const std::string_view file_name;
  const std::string_view function_name;
  std::atomic<std::uint_least32_t> line;
};

}  // namespace arangodb::basics
