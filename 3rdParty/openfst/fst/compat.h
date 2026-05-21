// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.

#ifndef FST_COMPAT_H_
#define FST_COMPAT_H_

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define OPENFST_DEPRECATED(message) __attribute__((deprecated(message)))
#elif defined(_MSC_VER)
#define OPENFST_DEPRECATED(message) [[deprecated(message)]]
#else
#define OPENFST_DEPRECATED(message)
#endif

void FailedNewHandler();

namespace fst {

// Downcasting.

template <typename To, typename From>
inline To down_cast(From *f) {
  return static_cast<To>(f);
}

template <typename To, typename From>
inline To down_cast(From &f) {
  return static_cast<To>(f);
}

// Bitcasting.
template <class Dest, class Source>
inline Dest bit_cast(const Source &source) {
  static_assert(sizeof(Dest) == sizeof(Source),
                "Bitcasting unsafe for specified types");
  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

namespace internal {

// TODO(kbg): Remove this once we migrate to C++20.
template <typename T>
struct type_identity {
  using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

}  // namespace internal

template <typename To>
constexpr To implicit_cast(typename internal::type_identity_t<To> to) {
  return to;
}

// Checksums.
class CheckSummer {
 public:
  CheckSummer();

  void Reset();

  void Update(std::string_view data);

  std::string Digest() { return check_sum_; }

 private:
  static constexpr int kCheckSumLength = 32;
  int count_;
  std::string check_sum_;

  CheckSummer(const CheckSummer &) = delete;
  CheckSummer &operator=(const CheckSummer &) = delete;
};

// Defines make_unique_for_overwrite using a standard definition that should be
// compatible with the C++20 definition. That is, all compiling uses of
// `std::make_unique_for_overwrite` should have the same result with
// `fst::make_unique_for_overwrite`. Note that the reverse doesn't
// necessarily hold.
// TODO(kbg): Remove these once we migrate to C++20.

template <typename T>
std::unique_ptr<T> make_unique_for_overwrite() {
  return std::unique_ptr<T>(new T);
}

template <typename T>
std::unique_ptr<T> make_unique_for_overwrite(size_t n) {
  return std::unique_ptr<T>(new std::remove_extent_t<T>[n]);
}

template <typename T>
std::unique_ptr<T> WrapUnique(T *ptr) {
  return std::unique_ptr<T>(ptr);
}

// Range utilities

// A range adaptor for a pair of iterators.
//
// This just wraps two iterators into a range-compatible interface. Nothing
// fancy at all.
template <typename IteratorT>
class iterator_range {
 public:
  using iterator = IteratorT;
  using const_iterator = IteratorT;
  using value_type = typename std::iterator_traits<IteratorT>::value_type;

  iterator_range() : begin_iterator_(), end_iterator_() {}
  iterator_range(IteratorT begin_iterator, IteratorT end_iterator)
      : begin_iterator_(std::move(begin_iterator)),
        end_iterator_(std::move(end_iterator)) {}

  IteratorT begin() const { return begin_iterator_; }
  IteratorT end() const { return end_iterator_; }

 private:
  IteratorT begin_iterator_, end_iterator_;
};

// Convenience function for iterating over sub-ranges.
//
// This provides a bit of syntactic sugar to make using sub-ranges
// in for loops a bit easier. Analogous to std::make_pair().
template <typename T>
iterator_range<T> make_range(T x, T y) {
  return iterator_range<T>(std::move(x), std::move(y));
}

// String munging.

namespace internal {

// Computes size of joined string.
template <class S>
size_t GetResultSize(const std::vector<S> &elements, size_t s_size) {
  const auto lambda = [](size_t partial, const S &right) {
    return partial + right.size();
  };
  return std::accumulate(elements.begin(), elements.end(), 0, lambda) +
         elements.size() * s_size - s_size;
}

}  // namespace internal

template <class S>
std::string StringJoin(const std::vector<S> &elements, std::string_view delim) {
  std::string result;
  if (elements.empty()) return result;
  const size_t s_size = delim.size();
  result.reserve(internal::GetResultSize(elements, s_size));
  auto it = elements.begin();
  result.append(it->data(), it->size());
  for (++it; it != elements.end(); ++it) {
    result.append(delim.data(), s_size);
    result.append(it->data(), it->size());
  }
  return result;
}

template <class S>
std::string StringJoin(const std::vector<S> &elements, char delim) {
  const std::string_view view_delim(&delim, 1);
  return StringJoin(elements, view_delim);
}

struct SkipEmpty {};

struct ByAnyChar {
 public:
  explicit ByAnyChar(std::string_view sp) : delimiters(sp) {}

  std::string delimiters;
};

namespace internal {

class StringSplitter {
 public:
  using const_iterator = std::vector<std::string_view>::const_iterator;
  using value_type = std::string_view;

  StringSplitter(std::string_view string, std::string delim,
                 bool skip_empty = false)
      : string_(std::move(string)),
        delim_(std::move(delim)),
        skip_empty_(skip_empty),
        vec_(SplitToSv()) {}

  inline operator  // NOLINT(google-explicit-constructor)
      std::vector<std::string_view>() && {
    return std::move(vec_);
  }

  inline operator  // NOLINT(google-explicit-constructor)
      std::vector<std::string>() {
    std::vector<std::string> str_vec(vec_.begin(), vec_.end());
    return str_vec;
  }

  const_iterator begin() const { return vec_.begin(); }
  const_iterator end() const { return vec_.end(); }

 private:
  std::vector<std::string_view> SplitToSv();

  std::string_view string_;
  std::string delim_;
  bool skip_empty_;
  std::vector<std::string_view> vec_;
};

}  // namespace internal

// `StrSplit` replacements. Only support splitting on `char` or
// `ByAnyChar` (notable not on a multi-char string delimiter), and with or
// without `SkipEmpty`.
internal::StringSplitter StrSplit(std::string_view full, ByAnyChar delim);
internal::StringSplitter StrSplit(std::string_view full, char delim);
internal::StringSplitter StrSplit(std::string_view full, ByAnyChar delim,
                                  SkipEmpty);
internal::StringSplitter StrSplit(std::string_view full, char delim, SkipEmpty);

void StripTrailingAsciiWhitespace(std::string *full);

std::string_view StripTrailingAsciiWhitespace(std::string_view full);

class StringOrInt {
 public:
  template <typename T, typename = std::enable_if_t<
                            std::is_convertible_v<T, std::string_view>>>
  StringOrInt(T s) : str_(std::string(s)) {}  // NOLINT

  StringOrInt(int i) {  // NOLINT
    str_ = std::to_string(i);
  }

  const std::string &Get() const { return str_; }

 private:
  std::string str_;
};

// TODO(kbg): Make this work with variadic template, maybe.

inline std::string StrCat(const StringOrInt &s1, const StringOrInt &s2) {
  return s1.Get() + s2.Get();
}

inline std::string StrCat(const StringOrInt &s1, const StringOrInt &s2,
                          const StringOrInt &s3) {
  return s1.Get() + StrCat(s2, s3);
}

inline std::string StrCat(const StringOrInt &s1, const StringOrInt &s2,
                          const StringOrInt &s3, const StringOrInt &s4) {
  return s1.Get() + StrCat(s2, s3, s4);
}

inline std::string StrCat(const StringOrInt &s1, const StringOrInt &s2,
                          const StringOrInt &s3, const StringOrInt &s4,
                          const StringOrInt &s5) {
  return s1.Get() + StrCat(s2, s3, s4, s5);
}

// TODO(agutkin): Remove this once we migrate to C++20, where `starts_with`
// is available.
inline bool StartsWith(std::string_view text, std::string_view prefix) {
  return prefix.empty() ||
         (text.size() >= prefix.size() &&
          memcmp(text.data(), prefix.data(), prefix.size()) == 0);
}

inline bool ConsumePrefix(std::string_view *s, std::string_view expected) {
  if (!StartsWith(*s, expected)) return false;
  s->remove_prefix(expected.size());
  return true;
}

}  // namespace fst

#endif  // FST_COMPAT_H_
