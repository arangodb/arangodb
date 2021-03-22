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

#include <string.h>

#include <cctype>
#include <cstdint>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/time/internal/cctz/include/cctz/time_zone.h"
#include "absl/time/time.h"

namespace cctz = iresearch_absl::time_internal::cctz;

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN

IRESEARCH_ABSL_DLL extern const char RFC3339_full[] = "%Y-%m-%d%ET%H:%M:%E*S%Ez";
IRESEARCH_ABSL_DLL extern const char RFC3339_sec[] = "%Y-%m-%d%ET%H:%M:%S%Ez";

IRESEARCH_ABSL_DLL extern const char RFC1123_full[] = "%a, %d %b %E4Y %H:%M:%S %z";
IRESEARCH_ABSL_DLL extern const char RFC1123_no_wday[] = "%d %b %E4Y %H:%M:%S %z";

namespace {

const char kInfiniteFutureStr[] = "infinite-future";
const char kInfinitePastStr[] = "infinite-past";

struct cctz_parts {
  cctz::time_point<cctz::seconds> sec;
  cctz::detail::femtoseconds fem;
};

inline cctz::time_point<cctz::seconds> unix_epoch() {
  return std::chrono::time_point_cast<cctz::seconds>(
      std::chrono::system_clock::from_time_t(0));
}

// Splits a Time into seconds and femtoseconds, which can be used with CCTZ.
// Requires that 't' is finite. See duration.cc for details about rep_hi and
// rep_lo.
cctz_parts Split(iresearch_absl::Time t) {
  const auto d = time_internal::ToUnixDuration(t);
  const int64_t rep_hi = time_internal::GetRepHi(d);
  const int64_t rep_lo = time_internal::GetRepLo(d);
  const auto sec = unix_epoch() + cctz::seconds(rep_hi);
  const auto fem = cctz::detail::femtoseconds(rep_lo * (1000 * 1000 / 4));
  return {sec, fem};
}

// Joins the given seconds and femtoseconds into a Time. See duration.cc for
// details about rep_hi and rep_lo.
absl::Time Join(const cctz_parts& parts) {
  const int64_t rep_hi = (parts.sec - unix_epoch()).count();
  const uint32_t rep_lo = parts.fem.count() / (1000 * 1000 / 4);
  const auto d = time_internal::MakeDuration(rep_hi, rep_lo);
  return time_internal::FromUnixDuration(d);
}

}  // namespace

std::string FormatTime(iresearch_absl::string_view format, iresearch_absl::Time t,
                       iresearch_absl::TimeZone tz) {
  if (t == iresearch_absl::InfiniteFuture()) return std::string(kInfiniteFutureStr);
  if (t == iresearch_absl::InfinitePast()) return std::string(kInfinitePastStr);
  const auto parts = Split(t);
  return cctz::detail::format(std::string(format), parts.sec, parts.fem,
                              cctz::time_zone(tz));
}

std::string FormatTime(iresearch_absl::Time t, iresearch_absl::TimeZone tz) {
  return FormatTime(RFC3339_full, t, tz);
}

std::string FormatTime(iresearch_absl::Time t) {
  return iresearch_absl::FormatTime(RFC3339_full, t, iresearch_absl::LocalTimeZone());
}

bool ParseTime(iresearch_absl::string_view format, iresearch_absl::string_view input,
               iresearch_absl::Time* time, std::string* err) {
  return iresearch_absl::ParseTime(format, input, iresearch_absl::UTCTimeZone(), time, err);
}

// If the input string does not contain an explicit UTC offset, interpret
// the fields with respect to the given TimeZone.
bool ParseTime(iresearch_absl::string_view format, iresearch_absl::string_view input,
               iresearch_absl::TimeZone tz, iresearch_absl::Time* time, std::string* err) {
  auto strip_leading_space = [](iresearch_absl::string_view* sv) {
    while (!sv->empty()) {
      if (!std::isspace(sv->front())) return;
      sv->remove_prefix(1);
    }
  };

  // Portable toolchains means we don't get nice constexpr here.
  struct Literal {
    const char* name;
    size_t size;
    iresearch_absl::Time value;
  };
  static Literal literals[] = {
      {kInfiniteFutureStr, strlen(kInfiniteFutureStr), InfiniteFuture()},
      {kInfinitePastStr, strlen(kInfinitePastStr), InfinitePast()},
  };
  strip_leading_space(&input);
  for (const auto& lit : literals) {
    if (iresearch_absl::StartsWith(input, iresearch_absl::string_view(lit.name, lit.size))) {
      iresearch_absl::string_view tail = input;
      tail.remove_prefix(lit.size);
      strip_leading_space(&tail);
      if (tail.empty()) {
        *time = lit.value;
        return true;
      }
    }
  }

  std::string error;
  cctz_parts parts;
  const bool b =
      cctz::detail::parse(std::string(format), std::string(input),
                          cctz::time_zone(tz), &parts.sec, &parts.fem, &error);
  if (b) {
    *time = Join(parts);
  } else if (err != nullptr) {
    *err = error;
  }
  return b;
}

// Functions required to support iresearch_absl::Time flags.
bool AbslParseFlag(iresearch_absl::string_view text, iresearch_absl::Time* t, std::string* error) {
  return iresearch_absl::ParseTime(RFC3339_full, text, iresearch_absl::UTCTimeZone(), t, error);
}

std::string AbslUnparseFlag(iresearch_absl::Time t) {
  return iresearch_absl::FormatTime(RFC3339_full, t, iresearch_absl::UTCTimeZone());
}
bool ParseFlag(const std::string& text, iresearch_absl::Time* t, std::string* error) {
  return iresearch_absl::ParseTime(RFC3339_full, text, iresearch_absl::UTCTimeZone(), t, error);
}

std::string UnparseFlag(iresearch_absl::Time t) {
  return iresearch_absl::FormatTime(RFC3339_full, t, iresearch_absl::UTCTimeZone());
}

IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl
