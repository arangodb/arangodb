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

#ifndef IRESEARCH_ABSL_BASE_INTERNAL_LOG_SEVERITY_H_
#define IRESEARCH_ABSL_BASE_INTERNAL_LOG_SEVERITY_H_

#include <array>
#include <ostream>

#include "absl/base/attributes.h"
#include "absl/base/config.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN

// iresearch_absl::LogSeverity
//
// Four severity levels are defined. Logging APIs should terminate the program
// when a message is logged at severity `kFatal`; the other levels have no
// special semantics.
//
// Values other than the four defined levels (e.g. produced by `static_cast`)
// are valid, but their semantics when passed to a function, macro, or flag
// depend on the function, macro, or flag. The usual behavior is to normalize
// such values to a defined severity level, however in some cases values other
// than the defined levels are useful for comparison.
//
// Exmaple:
//
//   // Effectively disables all logging:
//   SetMinLogLevel(static_cast<iresearch_absl::LogSeverity>(100));
//
// Abseil flags may be defined with type `LogSeverity`. Dependency layering
// constraints require that the `AbslParseFlag()` overload be declared and
// defined in the flags library itself rather than here. The `AbslUnparseFlag()`
// overload is defined there as well for consistency.
//
// iresearch_absl::LogSeverity Flag String Representation
//
// An `absl::LogSeverity` has a string representation used for parsing
// command-line flags based on the enumerator name (e.g. `kFatal`) or
// its unprefixed name (without the `k`) in any case-insensitive form. (E.g.
// "FATAL", "fatal" or "Fatal" are all valid.) Unparsing such flags produces an
// unprefixed string representation in all caps (e.g. "FATAL") or an integer.
//
// Additionally, the parser accepts arbitrary integers (as if the type were
// `int`).
//
// Examples:
//
//   --my_log_level=kInfo
//   --my_log_level=INFO
//   --my_log_level=info
//   --my_log_level=0
//
// Unparsing a flag produces the same result as `absl::LogSeverityName()` for
// the standard levels and a base-ten integer otherwise.
enum class LogSeverity : int {
  kInfo = 0,
  kWarning = 1,
  kError = 2,
  kFatal = 3,
};

// LogSeverities()
//
// Returns an iterable of all standard `absl::LogSeverity` values, ordered from
// least to most severe.
constexpr std::array<iresearch_absl::LogSeverity, 4> LogSeverities() {
  return {{iresearch_absl::LogSeverity::kInfo, iresearch_absl::LogSeverity::kWarning,
           iresearch_absl::LogSeverity::kError, iresearch_absl::LogSeverity::kFatal}};
}

// LogSeverityName()
//
// Returns the all-caps string representation (e.g. "INFO") of the specified
// severity level if it is one of the standard levels and "UNKNOWN" otherwise.
constexpr const char* LogSeverityName(iresearch_absl::LogSeverity s) {
  return s == iresearch_absl::LogSeverity::kInfo
             ? "INFO"
             : s == iresearch_absl::LogSeverity::kWarning
                   ? "WARNING"
                   : s == iresearch_absl::LogSeverity::kError
                         ? "ERROR"
                         : s == iresearch_absl::LogSeverity::kFatal ? "FATAL" : "UNKNOWN";
}

// NormalizeLogSeverity()
//
// Values less than `kInfo` normalize to `kInfo`; values greater than `kFatal`
// normalize to `kError` (**NOT** `kFatal`).
constexpr iresearch_absl::LogSeverity NormalizeLogSeverity(iresearch_absl::LogSeverity s) {
  return s < iresearch_absl::LogSeverity::kInfo
             ? iresearch_absl::LogSeverity::kInfo
             : s > iresearch_absl::LogSeverity::kFatal ? iresearch_absl::LogSeverity::kError : s;
}
constexpr iresearch_absl::LogSeverity NormalizeLogSeverity(int s) {
  return iresearch_absl::NormalizeLogSeverity(static_cast<iresearch_absl::LogSeverity>(s));
}

// operator<<
//
// The exact representation of a streamed `absl::LogSeverity` is deliberately
// unspecified; do not rely on it.
std::ostream& operator<<(std::ostream& os, iresearch_absl::LogSeverity s);

IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_BASE_INTERNAL_LOG_SEVERITY_H_
