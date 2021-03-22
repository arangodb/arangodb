//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/strings/internal/str_format/extension.h"

#include <errno.h>
#include <algorithm>
#include <string>

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace str_format_internal {

std::string Flags::ToString() const {
  std::string s;
  s.append(left     ? "-" : "");
  s.append(show_pos ? "+" : "");
  s.append(sign_col ? " " : "");
  s.append(alt      ? "#" : "");
  s.append(zero     ? "0" : "");
  return s;
}

#define IRESEARCH_ABSL_INTERNAL_X_VAL(id) \
  constexpr iresearch_absl::FormatConversionChar FormatConversionCharInternal::id;
ABSL_INTERNAL_CONVERSION_CHARS_EXPAND_(IRESEARCH_ABSL_INTERNAL_X_VAL, )
#undef IRESEARCH_ABSL_INTERNAL_X_VAL
// NOLINTNEXTLINE(readability-redundant-declaration)
constexpr iresearch_absl::FormatConversionChar FormatConversionCharInternal::kNone;

#define IRESEARCH_ABSL_INTERNAL_CHAR_SET_CASE(c) \
  constexpr FormatConversionCharSet FormatConversionCharSetInternal::c;
ABSL_INTERNAL_CONVERSION_CHARS_EXPAND_(IRESEARCH_ABSL_INTERNAL_CHAR_SET_CASE, )
#undef IRESEARCH_ABSL_INTERNAL_CHAR_SET_CASE

// NOLINTNEXTLINE(readability-redundant-declaration)
constexpr FormatConversionCharSet FormatConversionCharSetInternal::kStar;
// NOLINTNEXTLINE(readability-redundant-declaration)
constexpr FormatConversionCharSet FormatConversionCharSetInternal::kIntegral;
// NOLINTNEXTLINE(readability-redundant-declaration)
constexpr FormatConversionCharSet FormatConversionCharSetInternal::kFloating;
// NOLINTNEXTLINE(readability-redundant-declaration)
constexpr FormatConversionCharSet FormatConversionCharSetInternal::kNumeric;
// NOLINTNEXTLINE(readability-redundant-declaration)
constexpr FormatConversionCharSet FormatConversionCharSetInternal::kPointer;

bool FormatSinkImpl::PutPaddedString(string_view value, int width,
                                     int precision, bool left) {
  size_t space_remaining = 0;
  if (width >= 0) space_remaining = width;
  size_t n = value.size();
  if (precision >= 0) n = std::min(n, static_cast<size_t>(precision));
  string_view shown(value.data(), n);
  space_remaining = Excess(shown.size(), space_remaining);
  if (!left) Append(space_remaining, ' ');
  Append(shown);
  if (left) Append(space_remaining, ' ');
  return true;
}

}  // namespace str_format_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl
