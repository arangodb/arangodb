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

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include "Basics/ResultT.h"

#include <unicode/regex.h>
#include <unicode/unistr.h>

namespace arangodb {
struct ICancellationToken;
namespace basics {

class IcuRegexMatcher {
 public:
  static std::unique_ptr<IcuRegexMatcher> create(std::string const& pattern);
  std::unique_ptr<IcuRegexMatcher> clone() const;
  void abortWhenKilled(ICancellationToken const* token);
  [[nodiscard]] ResultT<bool> matches(std::string_view value);
  [[nodiscard]] ResultT<bool> find(std::string_view value);
  int32_t groupCount() const;
  [[nodiscard]] ResultT<std::string> group(int32_t i);
  [[nodiscard]] ResultT<int32_t> split(icu_64_64::UnicodeString const& value,
                                       icu_64_64::UnicodeString* out,
                                       int32_t capacity);
  [[nodiscard]] ResultT<std::string> replace(std::string_view value,
                                             std::string_view replacement,
                                             bool all);

 private:
  explicit IcuRegexMatcher(std::unique_ptr<icu_64_64::RegexMatcher> m);
  Result check(UErrorCode status);

  std::unique_ptr<icu_64_64::RegexMatcher> _matcher;
  icu_64_64::UnicodeString _input;
};

}  // namespace basics
}  // namespace arangodb