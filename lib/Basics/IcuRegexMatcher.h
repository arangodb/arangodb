#pragma once

#include <cstdint>
#include <memory>
#include <optional>
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
  [[nodiscard]] ResultT<std::string> group(int32_t t);
  [[nodiscard]] ResultT<int32_t> split(icu_64_64::UnicodeString const& value,
                                       icu_64_64::UnicodeString* out,
                                       int32_t capacity);
  [[nodiscard]] ResultT<std::string> replace(std::string_view value,
                                             std::string_view replacement,
                                             bool all);

 private:
  explicit IcuRegexMatcher(std::unique_ptr<icu_64_64::RegexMatcher> m);
  Result check(UErrorCode status);

  std::unique_ptr<icu::RegexMatcher> _matcher;
  icu_64_64::UnicodeString _input;
};

}  // namespace basics
}  // namespace arangodb