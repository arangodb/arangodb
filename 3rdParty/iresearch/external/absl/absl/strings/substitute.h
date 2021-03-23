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
//
// -----------------------------------------------------------------------------
// File: substitute.h
// -----------------------------------------------------------------------------
//
// This package contains functions for efficiently performing string
// substitutions using a format string with positional notation:
// `Substitute()` and `SubstituteAndAppend()`.
//
// Unlike printf-style format specifiers, `Substitute()` functions do not need
// to specify the type of the substitution arguments. Supported arguments
// following the format string, such as strings, string_views, ints,
// floats, and bools, are automatically converted to strings during the
// substitution process. (See below for a full list of supported types.)
//
// `Substitute()` does not allow you to specify *how* to format a value, beyond
// the default conversion to string. For example, you cannot format an integer
// in hex.
//
// The format string uses positional identifiers indicated by a dollar sign ($)
// and single digit positional ids to indicate which substitution arguments to
// use at that location within the format string.
//
// A '$$' sequence in the format string causes a literal '$' character to be
// output.
//
// Example 1:
//   std::string s = Substitute("$1 purchased $0 $2 for $$10. Thanks $1!",
//                              5, "Bob", "Apples");
//   EXPECT_EQ("Bob purchased 5 Apples for $10. Thanks Bob!", s);
//
// Example 2:
//   std::string s = "Hi. ";
//   SubstituteAndAppend(&s, "My name is $0 and I am $1 years old.", "Bob", 5);
//   EXPECT_EQ("Hi. My name is Bob and I am 5 years old.", s);
//
// Supported types:
//   * iresearch_absl::string_view, std::string, const char* (null is equivalent to "")
//   * int32_t, int64_t, uint32_t, uint64_t
//   * float, double
//   * bool (Printed as "true" or "false")
//   * pointer types other than char* (Printed as "0x<lower case hex string>",
//     except that null is printed as "NULL")
//
// If an invalid format string is provided, Substitute returns an empty string
// and SubstituteAndAppend does not change the provided output string.
// A format string is invalid if it:
//   * ends in an unescaped $ character,
//     e.g. "Hello $", or
//   * calls for a position argument which is not provided,
//     e.g. Substitute("Hello $2", "world"), or
//   * specifies a non-digit, non-$ character after an unescaped $ character,
//     e.g. "Hello $f".
// In debug mode, i.e. #ifndef NDEBUG, such errors terminate the program.

#ifndef IRESEARCH_ABSL_STRINGS_SUBSTITUTE_H_
#define IRESEARCH_ABSL_STRINGS_SUBSTITUTE_H_

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/base/macros.h"
#include "absl/base/port.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace substitute_internal {

// Arg
//
// This class provides an argument type for `absl::Substitute()` and
// `absl::SubstituteAndAppend()`. `Arg` handles implicit conversion of various
// types to a string. (`Arg` is very similar to the `AlphaNum` class in
// `StrCat()`.)
//
// This class has implicit constructors.
class Arg {
 public:
  // Overloads for string-y things
  //
  // Explicitly overload `const char*` so the compiler doesn't cast to `bool`.
  Arg(const char* value)  // NOLINT(runtime/explicit)
      : piece_(iresearch_absl::NullSafeStringView(value)) {}
  template <typename Allocator>
  Arg(  // NOLINT
      const std::basic_string<char, std::char_traits<char>, Allocator>&
          value) noexcept
      : piece_(value) {}
  Arg(iresearch_absl::string_view value)  // NOLINT(runtime/explicit)
      : piece_(value) {}

  // Overloads for primitives
  //
  // No overloads are available for signed and unsigned char because if people
  // are explicitly declaring their chars as signed or unsigned then they are
  // probably using them as 8-bit integers and would probably prefer an integer
  // representation. However, we can't really know, so we make the caller decide
  // what to do.
  Arg(char value)  // NOLINT(runtime/explicit)
      : piece_(scratch_, 1) {
    scratch_[0] = value;
  }
  Arg(short value)  // NOLINT(*)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(unsigned short value)  // NOLINT(*)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(int value)  // NOLINT(runtime/explicit)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(unsigned int value)  // NOLINT(runtime/explicit)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(long value)  // NOLINT(*)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(unsigned long value)  // NOLINT(*)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(long long value)  // NOLINT(*)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(unsigned long long value)  // NOLINT(*)
      : piece_(scratch_,
               numbers_internal::FastIntToBuffer(value, scratch_) - scratch_) {}
  Arg(float value)  // NOLINT(runtime/explicit)
      : piece_(scratch_, numbers_internal::SixDigitsToBuffer(value, scratch_)) {
  }
  Arg(double value)  // NOLINT(runtime/explicit)
      : piece_(scratch_, numbers_internal::SixDigitsToBuffer(value, scratch_)) {
  }
  Arg(bool value)  // NOLINT(runtime/explicit)
      : piece_(value ? "true" : "false") {}

  Arg(Hex hex);  // NOLINT(runtime/explicit)
  Arg(Dec dec);  // NOLINT(runtime/explicit)

  // vector<bool>::reference and const_reference require special help to
  // convert to `AlphaNum` because it requires two user defined conversions.
  template <typename T,
            iresearch_absl::enable_if_t<
                std::is_class<T>::value &&
                (std::is_same<T, std::vector<bool>::reference>::value ||
                 std::is_same<T, std::vector<bool>::const_reference>::value)>* =
                nullptr>
  Arg(T value)  // NOLINT(google-explicit-constructor)
      : Arg(static_cast<bool>(value)) {}

  // `void*` values, with the exception of `char*`, are printed as
  // "0x<hex value>". However, in the case of `nullptr`, "NULL" is printed.
  Arg(const void* value);  // NOLINT(runtime/explicit)

  Arg(const Arg&) = delete;
  Arg& operator=(const Arg&) = delete;

  iresearch_absl::string_view piece() const { return piece_; }

 private:
  iresearch_absl::string_view piece_;
  char scratch_[numbers_internal::kFastToBufferSize];
};

// Internal helper function. Don't call this from outside this implementation.
// This interface may change without notice.
void SubstituteAndAppendArray(std::string* output, iresearch_absl::string_view format,
                              const iresearch_absl::string_view* args_array,
                              size_t num_args);

#if defined(IRESEARCH_ABSL_BAD_CALL_IF)
constexpr int CalculateOneBit(const char* format) {
  // Returns:
  // * 2^N for '$N' when N is in [0-9]
  // * 0 for correct '$' escaping: '$$'.
  // * -1 otherwise.
  return (*format < '0' || *format > '9') ? (*format == '$' ? 0 : -1)
                                          : (1 << (*format - '0'));
}

constexpr const char* SkipNumber(const char* format) {
  return !*format ? format : (format + 1);
}

constexpr int PlaceholderBitmask(const char* format) {
  return !*format
             ? 0
             : *format != '$' ? PlaceholderBitmask(format + 1)
                              : (CalculateOneBit(format + 1) |
                                 PlaceholderBitmask(SkipNumber(format + 1)));
}
#endif  // IRESEARCH_ABSL_BAD_CALL_IF

}  // namespace substitute_internal

//
// PUBLIC API
//

// SubstituteAndAppend()
//
// Substitutes variables into a given format string and appends to a given
// output string. See file comments above for usage.
//
// The declarations of `SubstituteAndAppend()` below consist of overloads
// for passing 0 to 10 arguments, respectively.
//
// NOTE: A zero-argument `SubstituteAndAppend()` may be used within variadic
// templates to allow a variable number of arguments.
//
// Example:
//  template <typename... Args>
//  void VarMsg(std::string* boilerplate, iresearch_absl::string_view format,
//      const Args&... args) {
//    iresearch_absl::SubstituteAndAppend(boilerplate, format, args...);
//  }
//
inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format) {
  substitute_internal::SubstituteAndAppendArray(output, format, nullptr, 0);
}

inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format,
                                const substitute_internal::Arg& a0) {
  const iresearch_absl::string_view args[] = {a0.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format,
                                const substitute_internal::Arg& a0,
                                const substitute_internal::Arg& a1) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format,
                                const substitute_internal::Arg& a0,
                                const substitute_internal::Arg& a1,
                                const substitute_internal::Arg& a2) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece(), a2.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format,
                                const substitute_internal::Arg& a0,
                                const substitute_internal::Arg& a1,
                                const substitute_internal::Arg& a2,
                                const substitute_internal::Arg& a3) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece(), a2.piece(),
                                    a3.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format,
                                const substitute_internal::Arg& a0,
                                const substitute_internal::Arg& a1,
                                const substitute_internal::Arg& a2,
                                const substitute_internal::Arg& a3,
                                const substitute_internal::Arg& a4) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece(), a2.piece(),
                                    a3.piece(), a4.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format,
                                const substitute_internal::Arg& a0,
                                const substitute_internal::Arg& a1,
                                const substitute_internal::Arg& a2,
                                const substitute_internal::Arg& a3,
                                const substitute_internal::Arg& a4,
                                const substitute_internal::Arg& a5) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece(), a2.piece(),
                                    a3.piece(), a4.piece(), a5.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(std::string* output, iresearch_absl::string_view format,
                                const substitute_internal::Arg& a0,
                                const substitute_internal::Arg& a1,
                                const substitute_internal::Arg& a2,
                                const substitute_internal::Arg& a3,
                                const substitute_internal::Arg& a4,
                                const substitute_internal::Arg& a5,
                                const substitute_internal::Arg& a6) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece(), a2.piece(),
                                    a3.piece(), a4.piece(), a5.piece(),
                                    a6.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(
    std::string* output, iresearch_absl::string_view format,
    const substitute_internal::Arg& a0, const substitute_internal::Arg& a1,
    const substitute_internal::Arg& a2, const substitute_internal::Arg& a3,
    const substitute_internal::Arg& a4, const substitute_internal::Arg& a5,
    const substitute_internal::Arg& a6, const substitute_internal::Arg& a7) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece(), a2.piece(),
                                    a3.piece(), a4.piece(), a5.piece(),
                                    a6.piece(), a7.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(
    std::string* output, iresearch_absl::string_view format,
    const substitute_internal::Arg& a0, const substitute_internal::Arg& a1,
    const substitute_internal::Arg& a2, const substitute_internal::Arg& a3,
    const substitute_internal::Arg& a4, const substitute_internal::Arg& a5,
    const substitute_internal::Arg& a6, const substitute_internal::Arg& a7,
    const substitute_internal::Arg& a8) {
  const iresearch_absl::string_view args[] = {a0.piece(), a1.piece(), a2.piece(),
                                    a3.piece(), a4.piece(), a5.piece(),
                                    a6.piece(), a7.piece(), a8.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

inline void SubstituteAndAppend(
    std::string* output, iresearch_absl::string_view format,
    const substitute_internal::Arg& a0, const substitute_internal::Arg& a1,
    const substitute_internal::Arg& a2, const substitute_internal::Arg& a3,
    const substitute_internal::Arg& a4, const substitute_internal::Arg& a5,
    const substitute_internal::Arg& a6, const substitute_internal::Arg& a7,
    const substitute_internal::Arg& a8, const substitute_internal::Arg& a9) {
  const iresearch_absl::string_view args[] = {
      a0.piece(), a1.piece(), a2.piece(), a3.piece(), a4.piece(),
      a5.piece(), a6.piece(), a7.piece(), a8.piece(), a9.piece()};
  substitute_internal::SubstituteAndAppendArray(output, format, args,
                                                IRESEARCH_ABSL_ARRAYSIZE(args));
}

#if defined(IRESEARCH_ABSL_BAD_CALL_IF)
// This body of functions catches cases where the number of placeholders
// doesn't match the number of data arguments.
void SubstituteAndAppend(std::string* output, const char* format)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 0,
                     "There were no substitution arguments "
                     "but this format string has a $[0-9] in it");

void SubstituteAndAppend(std::string* output, const char* format,
                         const substitute_internal::Arg& a0)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 1,
                     "There was 1 substitution argument given, but "
                     "this format string is either missing its $0, or "
                     "contains one of $1-$9");

void SubstituteAndAppend(std::string* output, const char* format,
                         const substitute_internal::Arg& a0,
                         const substitute_internal::Arg& a1)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 3,
                     "There were 2 substitution arguments given, but "
                     "this format string is either missing its $0/$1, or "
                     "contains one of $2-$9");

void SubstituteAndAppend(std::string* output, const char* format,
                         const substitute_internal::Arg& a0,
                         const substitute_internal::Arg& a1,
                         const substitute_internal::Arg& a2)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 7,
                     "There were 3 substitution arguments given, but "
                     "this format string is either missing its $0/$1/$2, or "
                     "contains one of $3-$9");

void SubstituteAndAppend(std::string* output, const char* format,
                         const substitute_internal::Arg& a0,
                         const substitute_internal::Arg& a1,
                         const substitute_internal::Arg& a2,
                         const substitute_internal::Arg& a3)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 15,
                     "There were 4 substitution arguments given, but "
                     "this format string is either missing its $0-$3, or "
                     "contains one of $4-$9");

void SubstituteAndAppend(std::string* output, const char* format,
                         const substitute_internal::Arg& a0,
                         const substitute_internal::Arg& a1,
                         const substitute_internal::Arg& a2,
                         const substitute_internal::Arg& a3,
                         const substitute_internal::Arg& a4)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 31,
                     "There were 5 substitution arguments given, but "
                     "this format string is either missing its $0-$4, or "
                     "contains one of $5-$9");

void SubstituteAndAppend(std::string* output, const char* format,
                         const substitute_internal::Arg& a0,
                         const substitute_internal::Arg& a1,
                         const substitute_internal::Arg& a2,
                         const substitute_internal::Arg& a3,
                         const substitute_internal::Arg& a4,
                         const substitute_internal::Arg& a5)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 63,
                     "There were 6 substitution arguments given, but "
                     "this format string is either missing its $0-$5, or "
                     "contains one of $6-$9");

void SubstituteAndAppend(
    std::string* output, const char* format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 127,
                     "There were 7 substitution arguments given, but "
                     "this format string is either missing its $0-$6, or "
                     "contains one of $7-$9");

void SubstituteAndAppend(
    std::string* output, const char* format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 255,
                     "There were 8 substitution arguments given, but "
                     "this format string is either missing its $0-$7, or "
                     "contains one of $8-$9");

void SubstituteAndAppend(
    std::string* output, const char* format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7, const substitute_internal::Arg& a8)
    IRESEARCH_ABSL_BAD_CALL_IF(
        substitute_internal::PlaceholderBitmask(format) != 511,
        "There were 9 substitution arguments given, but "
        "this format string is either missing its $0-$8, or contains a $9");

void SubstituteAndAppend(
    std::string* output, const char* format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7, const substitute_internal::Arg& a8,
    const substitute_internal::Arg& a9)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 1023,
                     "There were 10 substitution arguments given, but this "
                     "format string doesn't contain all of $0 through $9");
#endif  // IRESEARCH_ABSL_BAD_CALL_IF

// Substitute()
//
// Substitutes variables into a given format string. See file comments above
// for usage.
//
// The declarations of `Substitute()` below consist of overloads for passing 0
// to 10 arguments, respectively.
//
// NOTE: A zero-argument `Substitute()` may be used within variadic templates to
// allow a variable number of arguments.
//
// Example:
//  template <typename... Args>
//  void VarMsg(iresearch_absl::string_view format, const Args&... args) {
//    std::string s = iresearch_absl::Substitute(format, args...);

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(iresearch_absl::string_view format) {
  std::string result;
  SubstituteAndAppend(&result, format);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0) {
  std::string result;
  SubstituteAndAppend(&result, format, a0);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2, a3);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2, a3, a4);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2, a3, a4, a5);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2, a3, a4, a5, a6);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2, a3, a4, a5, a6, a7);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7, const substitute_internal::Arg& a8) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2, a3, a4, a5, a6, a7, a8);
  return result;
}

IRESEARCH_ABSL_MUST_USE_RESULT inline std::string Substitute(
    iresearch_absl::string_view format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7, const substitute_internal::Arg& a8,
    const substitute_internal::Arg& a9) {
  std::string result;
  SubstituteAndAppend(&result, format, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
  return result;
}

#if defined(IRESEARCH_ABSL_BAD_CALL_IF)
// This body of functions catches cases where the number of placeholders
// doesn't match the number of data arguments.
std::string Substitute(const char* format)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 0,
                     "There were no substitution arguments "
                     "but this format string has a $[0-9] in it");

std::string Substitute(const char* format, const substitute_internal::Arg& a0)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 1,
                     "There was 1 substitution argument given, but "
                     "this format string is either missing its $0, or "
                     "contains one of $1-$9");

std::string Substitute(const char* format, const substitute_internal::Arg& a0,
                       const substitute_internal::Arg& a1)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 3,
                     "There were 2 substitution arguments given, but "
                     "this format string is either missing its $0/$1, or "
                     "contains one of $2-$9");

std::string Substitute(const char* format, const substitute_internal::Arg& a0,
                       const substitute_internal::Arg& a1,
                       const substitute_internal::Arg& a2)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 7,
                     "There were 3 substitution arguments given, but "
                     "this format string is either missing its $0/$1/$2, or "
                     "contains one of $3-$9");

std::string Substitute(const char* format, const substitute_internal::Arg& a0,
                       const substitute_internal::Arg& a1,
                       const substitute_internal::Arg& a2,
                       const substitute_internal::Arg& a3)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 15,
                     "There were 4 substitution arguments given, but "
                     "this format string is either missing its $0-$3, or "
                     "contains one of $4-$9");

std::string Substitute(const char* format, const substitute_internal::Arg& a0,
                       const substitute_internal::Arg& a1,
                       const substitute_internal::Arg& a2,
                       const substitute_internal::Arg& a3,
                       const substitute_internal::Arg& a4)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 31,
                     "There were 5 substitution arguments given, but "
                     "this format string is either missing its $0-$4, or "
                     "contains one of $5-$9");

std::string Substitute(const char* format, const substitute_internal::Arg& a0,
                       const substitute_internal::Arg& a1,
                       const substitute_internal::Arg& a2,
                       const substitute_internal::Arg& a3,
                       const substitute_internal::Arg& a4,
                       const substitute_internal::Arg& a5)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 63,
                     "There were 6 substitution arguments given, but "
                     "this format string is either missing its $0-$5, or "
                     "contains one of $6-$9");

std::string Substitute(const char* format, const substitute_internal::Arg& a0,
                       const substitute_internal::Arg& a1,
                       const substitute_internal::Arg& a2,
                       const substitute_internal::Arg& a3,
                       const substitute_internal::Arg& a4,
                       const substitute_internal::Arg& a5,
                       const substitute_internal::Arg& a6)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 127,
                     "There were 7 substitution arguments given, but "
                     "this format string is either missing its $0-$6, or "
                     "contains one of $7-$9");

std::string Substitute(const char* format, const substitute_internal::Arg& a0,
                       const substitute_internal::Arg& a1,
                       const substitute_internal::Arg& a2,
                       const substitute_internal::Arg& a3,
                       const substitute_internal::Arg& a4,
                       const substitute_internal::Arg& a5,
                       const substitute_internal::Arg& a6,
                       const substitute_internal::Arg& a7)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 255,
                     "There were 8 substitution arguments given, but "
                     "this format string is either missing its $0-$7, or "
                     "contains one of $8-$9");

std::string Substitute(
    const char* format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7, const substitute_internal::Arg& a8)
    IRESEARCH_ABSL_BAD_CALL_IF(
        substitute_internal::PlaceholderBitmask(format) != 511,
        "There were 9 substitution arguments given, but "
        "this format string is either missing its $0-$8, or contains a $9");

std::string Substitute(
    const char* format, const substitute_internal::Arg& a0,
    const substitute_internal::Arg& a1, const substitute_internal::Arg& a2,
    const substitute_internal::Arg& a3, const substitute_internal::Arg& a4,
    const substitute_internal::Arg& a5, const substitute_internal::Arg& a6,
    const substitute_internal::Arg& a7, const substitute_internal::Arg& a8,
    const substitute_internal::Arg& a9)
    IRESEARCH_ABSL_BAD_CALL_IF(substitute_internal::PlaceholderBitmask(format) != 1023,
                     "There were 10 substitution arguments given, but this "
                     "format string doesn't contain all of $0 through $9");
#endif  // IRESEARCH_ABSL_BAD_CALL_IF

IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_STRINGS_SUBSTITUTE_H_
