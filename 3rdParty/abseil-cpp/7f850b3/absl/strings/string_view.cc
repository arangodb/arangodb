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

#include "absl/strings/string_view.h"

#ifndef ABSL_USES_STD_STRING_VIEW

#include <algorithm>
#include <climits>
#include <cstring>
#include <ostream>

#include "absl/strings/internal/memutil.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace {
void WritePadding(std::ostream& o, size_t pad) {
  char fill_buf[32];
  memset(fill_buf, o.fill(), sizeof(fill_buf));
  while (pad) {
    size_t n = std::min(pad, sizeof(fill_buf));
    o.write(fill_buf, n);
    pad -= n;
  }
}

class LookupTable {
 public:
  // For each character in wanted, sets the index corresponding
  // to the ASCII code of that character. This is used by
  // the find_.*_of methods below to tell whether or not a character is in
  // the lookup table in constant time.
  explicit LookupTable(string_view wanted) {
    for (char c : wanted) {
      table_[Index(c)] = true;
    }
  }
  bool operator[](char c) const { return table_[Index(c)]; }

 private:
  static unsigned char Index(char c) { return static_cast<unsigned char>(c); }
  bool table_[UCHAR_MAX + 1] = {};
};

}  // namespace

std::ostream& operator<<(std::ostream& o, string_view piece) {
  std::ostream::sentry sentry(o);
  if (sentry) {
    size_t lpad = 0;
    size_t rpad = 0;
    if (static_cast<size_t>(o.width()) > piece.size()) {
      size_t pad = o.width() - piece.size();
      if ((o.flags() & o.adjustfield) == o.left) {
        rpad = pad;
      } else {
        lpad = pad;
      }
    }
    if (lpad) WritePadding(o, lpad);
    o.write(piece.data(), piece.size());
    if (rpad) WritePadding(o, rpad);
    o.width(0);
  }
  return o;
}

string_view::size_type string_view::find(string_view s,
                                         size_type pos) const noexcept {
  if (empty() || pos > length_) {
    if (empty() && pos == 0 && s.empty()) return 0;
    return npos;
  }
  const char* result =
      strings_internal::memmatch(ptr_ + pos, length_ - pos, s.ptr_, s.length_);
  return result ? result - ptr_ : npos;
}

string_view::size_type string_view::find(char c, size_type pos) const noexcept {
  if (empty() || pos >= length_) {
    return npos;
  }
  const char* result =
      static_cast<const char*>(memchr(ptr_ + pos, c, length_ - pos));
  return result != nullptr ? result - ptr_ : npos;
}

string_view::size_type string_view::rfind(string_view s,
                                          size_type pos) const noexcept {
  if (length_ < s.length_) return npos;
  if (s.empty()) return std::min(length_, pos);
  const char* last = ptr_ + std::min(length_ - s.length_, pos) + s.length_;
  const char* result = std::find_end(ptr_, last, s.ptr_, s.ptr_ + s.length_);
  return result != last ? result - ptr_ : npos;
}

// Search range is [0..pos] inclusive.  If pos == npos, search everything.
string_view::size_type string_view::rfind(char c,
                                          size_type pos) const noexcept {
  // Note: memrchr() is not available on Windows.
  if (empty()) return npos;
  for (size_type i = std::min(pos, length_ - 1);; --i) {
    if (ptr_[i] == c) {
      return i;
    }
    if (i == 0) break;
  }
  return npos;
}

string_view::size_type string_view::find_first_of(
    string_view s, size_type pos) const noexcept {
  if (empty() || s.empty()) {
    return npos;
  }
  // Avoid the cost of LookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_of(s.ptr_[0], pos);
  LookupTable tbl(s);
  for (size_type i = pos; i < length_; ++i) {
    if (tbl[ptr_[i]]) {
      return i;
    }
  }
  return npos;
}

string_view::size_type string_view::find_first_not_of(
    string_view s, size_type pos) const noexcept {
  if (empty()) return npos;
  // Avoid the cost of LookupTable() for a single-character search.
  if (s.length_ == 1) return find_first_not_of(s.ptr_[0], pos);
  LookupTable tbl(s);
  for (size_type i = pos; i < length_; ++i) {
    if (!tbl[ptr_[i]]) {
      return i;
    }
  }
  return npos;
}

string_view::size_type string_view::find_first_not_of(
    char c, size_type pos) const noexcept {
  if (empty()) return npos;
  for (; pos < length_; ++pos) {
    if (ptr_[pos] != c) {
      return pos;
    }
  }
  return npos;
}

string_view::size_type string_view::find_last_of(string_view s,
                                                 size_type pos) const noexcept {
  if (empty() || s.empty()) return npos;
  // Avoid the cost of LookupTable() for a single-character search.
  if (s.length_ == 1) return find_last_of(s.ptr_[0], pos);
  LookupTable tbl(s);
  for (size_type i = std::min(pos, length_ - 1);; --i) {
    if (tbl[ptr_[i]]) {
      return i;
    }
    if (i == 0) break;
  }
  return npos;
}

string_view::size_type string_view::find_last_not_of(
    string_view s, size_type pos) const noexcept {
  if (empty()) return npos;
  size_type i = std::min(pos, length_ - 1);
  if (s.empty()) return i;
  // Avoid the cost of LookupTable() for a single-character search.
  if (s.length_ == 1) return find_last_not_of(s.ptr_[0], pos);
  LookupTable tbl(s);
  for (;; --i) {
    if (!tbl[ptr_[i]]) {
      return i;
    }
    if (i == 0) break;
  }
  return npos;
}

string_view::size_type string_view::find_last_not_of(
    char c, size_type pos) const noexcept {
  if (empty()) return npos;
  size_type i = std::min(pos, length_ - 1);
  for (;; --i) {
    if (ptr_[i] != c) {
      return i;
    }
    if (i == 0) break;
  }
  return npos;
}

// MSVC has non-standard behavior that implicitly creates definitions for static
// const members. These implicit definitions conflict with explicit out-of-class
// member definitions that are required by the C++ standard, resulting in
// LNK1169 "multiply defined" errors at link time. __declspec(selectany) asks
// MSVC to choose only one definition for the symbol it decorates. See details
// at https://msdn.microsoft.com/en-us/library/34h23df8(v=vs.100).aspx
#ifdef _MSC_VER
#define ABSL_STRING_VIEW_SELECTANY __declspec(selectany)
#else
#define ABSL_STRING_VIEW_SELECTANY
#endif

ABSL_STRING_VIEW_SELECTANY
constexpr string_view::size_type string_view::npos;
ABSL_STRING_VIEW_SELECTANY
constexpr string_view::size_type string_view::kMaxSize;

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_USES_STD_STRING_VIEW
