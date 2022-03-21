// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "s2/third_party/absl/strings/str_cat.h"

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "s2/third_party/absl/strings/ascii.h"
#include "s2/third_party/absl/strings/internal/resize_uninitialized.h"

namespace absl {

AlphaNum::AlphaNum(Hex hex) {
  char* const end = &digits_[numbers_internal::kFastToBufferSize];
  char* writer = end;
  uint64_t value = hex.value;
  static const char hexdigits[] = "0123456789abcdef";
  do {
    *--writer = hexdigits[value & 0xF];
    value >>= 4;
  } while (value != 0);

  char* beg;
  if (end - writer < hex.width) {
    beg = end - hex.width;
    std::fill_n(beg, writer - beg, hex.fill);
  } else {
    beg = writer;
  }

  piece_ = absl::string_view(beg, end - beg);
}

AlphaNum::AlphaNum(Dec dec) {
  assert(dec.width <= numbers_internal::kFastToBufferSize);
  char* const end = &digits_[numbers_internal::kFastToBufferSize];
  char* const minfill = end - dec.width;
  char* writer = end;
  uint64_t value = dec.value;
  bool neg = dec.neg;
  while (value > 9) {
    *--writer = '0' + (value % 10);
    value /= 10;
  }
  *--writer = '0' + value;
  if (neg) *--writer = '-';

  ptrdiff_t fillers = writer - minfill;
  if (fillers > 0) {
    // Tricky: if the fill character is ' ', then it's <fill><+/-><digits>
    // But...: if the fill character is '0', then it's <+/-><fill><digits>
    bool add_sign_again = false;
    if (neg && dec.fill == '0') {  // If filling with '0',
      ++writer;                    // ignore the sign we just added
      add_sign_again = true;       // and re-add the sign later.
    }
    writer -= fillers;
    std::fill_n(writer, fillers, dec.fill);
    if (add_sign_again) *--writer = '-';
  }

  piece_ = absl::string_view(writer, end - writer);
}

// ----------------------------------------------------------------------
// StrCat()
//    This merges the given strings or integers, with no delimiter.  This
//    is designed to be the fastest possible way to construct a string out
//    of a mix of raw C strings, StringPieces, strings, and integer values.
// ----------------------------------------------------------------------

// Append is merely a version of memcpy that returns the address of the byte
// after the area just overwritten.
static char* Append(char* out, const AlphaNum& x) {
  // memcpy is allowed to overwrite arbitrary memory, so doing this after the
  // call would force an extra fetch of x.size().
  char* after = out + x.size();
  memcpy(out, x.data(), x.size());
  return after;
}

string StrCat(const AlphaNum& a, const AlphaNum& b) {
  string result;
  absl::strings_internal::STLStringResizeUninitialized(&result,
                                                       a.size() + b.size());
  char* const begin = &*result.begin();
  char* out = begin;
  out = Append(out, a);
  out = Append(out, b);
  assert(out == begin + result.size());
  return result;
}

string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c) {
  string result;
  strings_internal::STLStringResizeUninitialized(
      &result, a.size() + b.size() + c.size());
  char* const begin = &*result.begin();
  char* out = begin;
  out = Append(out, a);
  out = Append(out, b);
  out = Append(out, c);
  assert(out == begin + result.size());
  return result;
}

string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d) {
  string result;
  strings_internal::STLStringResizeUninitialized(
      &result, a.size() + b.size() + c.size() + d.size());
  char* const begin = &*result.begin();
  char* out = begin;
  out = Append(out, a);
  out = Append(out, b);
  out = Append(out, c);
  out = Append(out, d);
  assert(out == begin + result.size());
  return result;
}

namespace strings_internal {

// Do not call directly - these are not part of the public API.
string CatPieces(std::initializer_list<absl::string_view> pieces) {
  string result;
  size_t total_size = 0;
  for (const absl::string_view piece : pieces) total_size += piece.size();
  strings_internal::STLStringResizeUninitialized(&result, total_size);

  char* const begin = &*result.begin();
  char* out = begin;
  for (const absl::string_view piece : pieces) {
    const size_t this_size = piece.size();
    memcpy(out, piece.data(), this_size);
    out += this_size;
  }
  assert(out == begin + result.size());
  return result;
}

// It's possible to call StrAppend with an absl::string_view that is itself a
// fragment of the string we're appending to.  However the results of this are
// random. Therefore, check for this in debug mode.  Use unsigned math so we
// only have to do one comparison. Note, there's an exception case: appending an
// empty string is always allowed.
#define ASSERT_NO_OVERLAP(dest, src) \
  assert(((src).size() == 0) ||      \
         (uintptr_t((src).data() - (dest).data()) > uintptr_t((dest).size())))

void AppendPieces(string* dest,
                  std::initializer_list<absl::string_view> pieces) {
  size_t old_size = dest->size();
  size_t total_size = old_size;
  for (const absl::string_view piece : pieces) {
    ASSERT_NO_OVERLAP(*dest, piece);
    total_size += piece.size();
  }
  strings_internal::STLStringResizeUninitialized(dest, total_size);

  char* const begin = &*dest->begin();
  char* out = begin + old_size;
  for (const absl::string_view piece : pieces) {
    const size_t this_size = piece.size();
    memcpy(out, piece.data(), this_size);
    out += this_size;
  }
  assert(out == begin + dest->size());
}


}  // namespace strings_internal

void StrAppend(string* dest, const AlphaNum& a) {
  ASSERT_NO_OVERLAP(*dest, a);
  dest->append(a.data(), a.size());
}

void StrAppend(string* dest, const AlphaNum& a, const AlphaNum& b) {
  ASSERT_NO_OVERLAP(*dest, a);
  ASSERT_NO_OVERLAP(*dest, b);
  string::size_type old_size = dest->size();
  strings_internal::STLStringResizeUninitialized(
      dest, old_size + a.size() + b.size());
  char* const begin = &*dest->begin();
  char* out = begin + old_size;
  out = Append(out, a);
  out = Append(out, b);
  assert(out == begin + dest->size());
}

void StrAppend(string* dest, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c) {
  ASSERT_NO_OVERLAP(*dest, a);
  ASSERT_NO_OVERLAP(*dest, b);
  ASSERT_NO_OVERLAP(*dest, c);
  string::size_type old_size = dest->size();
  strings_internal::STLStringResizeUninitialized(
      dest, old_size + a.size() + b.size() + c.size());
  char* const begin = &*dest->begin();
  char* out = begin + old_size;
  out = Append(out, a);
  out = Append(out, b);
  out = Append(out, c);
  assert(out == begin + dest->size());
}

void StrAppend(string* dest, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d) {
  ASSERT_NO_OVERLAP(*dest, a);
  ASSERT_NO_OVERLAP(*dest, b);
  ASSERT_NO_OVERLAP(*dest, c);
  ASSERT_NO_OVERLAP(*dest, d);
  string::size_type old_size = dest->size();
  strings_internal::STLStringResizeUninitialized(
      dest, old_size + a.size() + b.size() + c.size() + d.size());
  char* const begin = &*dest->begin();
  char* out = begin + old_size;
  out = Append(out, a);
  out = Append(out, b);
  out = Append(out, c);
  out = Append(out, d);
  assert(out == begin + dest->size());
}

}  // namespace absl
