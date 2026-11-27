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

#include "IcuRegexMatcher.h"

#include "Basics/Exceptions.h"
#include "Basics/ICancellationToken.h"
#include "Basics/voc-errors.h"

#include <unicode/stringpiece.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

namespace arangodb::basics {
namespace {

icu_64_64::UnicodeString toUnicode(std::string_view s) {
  return icu_64_64::UnicodeString::fromUTF8(
      icu_64_64::StringPiece(s.data(), static_cast<int32_t>(s.size())));
}

}  // namespace

IcuRegexMatcher::IcuRegexMatcher(std::unique_ptr<icu_64_64::RegexMatcher> m)
    : _matcher(std::move(m)) {}

std::unique_ptr<IcuRegexMatcher> IcuRegexMatcher::create(
    std::string const& pattern) {
  UErrorCode status = U_ZERO_ERROR;
  auto m = std::make_unique<icu_64_64::RegexMatcher>(
      icu_64_64::UnicodeString::fromUTF8(pattern), 0, status);
  if (status == U_MEMORY_ALLOCATION_ERROR) {
    throw std::bad_alloc();
  }
  if (U_FAILURE(status)) {
    return nullptr;
  }
  return std::unique_ptr<IcuRegexMatcher>(new IcuRegexMatcher(std::move(m)));
}

std::unique_ptr<IcuRegexMatcher> IcuRegexMatcher::clone() const {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu_64_64::RegexMatcher> m(
      _matcher->pattern().matcher(status));
  if (m == nullptr || U_FAILURE(status)) {
    throw std::bad_alloc();
  }
  return std::unique_ptr<IcuRegexMatcher>(new IcuRegexMatcher(std::move(m)));
}

void IcuRegexMatcher::abortWhenKilled(ICancellationToken const* token) {
  if (token == nullptr) {
    return;
  }
  UErrorCode status = U_ZERO_ERROR;
  _matcher->setMatchCallback(
      [](void const* context, int32_t) -> UBool {
        return !static_cast<ICancellationToken const*>(context)->killed();
      },
      token, status);
}

ResultT<bool> IcuRegexMatcher::matches(std::string_view value) {
  _input = toUnicode(value);
  _matcher->reset(_input);
  UErrorCode status = U_ZERO_ERROR;
  bool matched = _matcher->matches(status);
  if (auto res = check(status); res.fail()) {
    return res;
  }
  return matched;
}

ResultT<bool> IcuRegexMatcher::find(std::string_view value) {
  _input = toUnicode(value);
  _matcher->reset(_input);
  UErrorCode status = U_ZERO_ERROR;
  bool found = _matcher->find(status);
  if (auto res = check(status); res.fail()) {
    return res;
  }
  return found;
}

int32_t IcuRegexMatcher::groupCount() const { return _matcher->groupCount(); }

ResultT<std::string> IcuRegexMatcher::group(int32_t i) {
  UErrorCode status = U_ZERO_ERROR;
  icu_64_64::UnicodeString g = _matcher->group(i, status);
  if (auto res = check(status); res.fail()) {
    return res;
  }
  std::string utf8;
  g.toUTF8String(utf8);
  return utf8;
}

ResultT<std::string> IcuRegexMatcher::replace(std::string_view value,
                                              std::string_view replacement,
                                              bool all) {
  _input = toUnicode(value);
  _matcher->reset(_input);
  UErrorCode status = U_ZERO_ERROR;
  icu_64_64::UnicodeString r = toUnicode(replacement);
  icu_64_64::UnicodeString result =
      all ? _matcher->replaceAll(r, status) : _matcher->replaceFirst(r, status);
  if (auto res = check(status); res.fail()) {
    return res;
  }
  std::string utf8;
  result.toUTF8String(utf8);
  return utf8;
}

ResultT<int32_t> IcuRegexMatcher::split(icu_64_64::UnicodeString const& value,
                                        icu_64_64::UnicodeString* out,
                                        int32_t capacity) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t count = _matcher->split(value, out, capacity, status);
  if (auto res = check(status); res.fail()) {
    return res;
  }
  return count;
}

Result IcuRegexMatcher::check(UErrorCode status) {
  if (status == U_MEMORY_ALLOCATION_ERROR) {
    throw std::bad_alloc();
  }
  if (status == U_REGEX_STOPPED_BY_CALLER) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
  if (U_FAILURE(status)) {
    return Result(TRI_ERROR_ARANGO_ICU_ERROR, u_errorName(status));
  }
  return {};
}

}  // namespace arangodb::basics