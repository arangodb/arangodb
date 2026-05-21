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
//
// FST utility inline definitions.

#ifndef FST_UTIL_H_
#define FST_UTIL_H_

#include <array>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fst/compat.h>
#include <fst/log.h>
#include <fstream>
#include <fst/mapped-file.h>

#include <fst/flags.h>
#include <unordered_map>
#include <string_view>
#include <optional>


// Utility for error handling.

DECLARE_bool(fst_error_fatal);

#define FSTERROR()                                                     \
  LOG(LEVEL(FST_FLAGS_fst_error_fatal ? base_logging::FATAL \
                                                 : base_logging::ERROR))

namespace fst {

// Utility for type I/O.

// Reads types from an input stream.

// Generic case.
template <class T, typename std::enable_if_t<std::is_class_v<T>, T> * = nullptr>
inline std::istream &ReadType(std::istream &strm, T *t) {
  return t->Read(strm);
}

// Numeric (boolean, integral, floating-point) case.
template <class T,
          typename std::enable_if_t<std::is_arithmetic_v<T>, T> * = nullptr>
inline std::istream &ReadType(std::istream &strm, T *t) {
  return strm.read(reinterpret_cast<char *>(t), sizeof(T));
}

// Numeric (boolean, integral, floating-point) case only.
template <class T>
inline std::istream &ReadTypeN(std::istream &strm, size_t n, T *t) {
  static_assert(std::is_arithmetic_v<T>, "Type not supported for batch read.");
  return strm.read(reinterpret_cast<char *>(t), sizeof(T) * n);
}

// String case.
inline std::istream &ReadType(std::istream &strm, std::string *s) {
  s->clear();
  int32_t ns = 0;
  ReadType(strm, &ns);
  if (ns <= 0) return strm;
  s->resize(ns);
  ReadTypeN(strm, ns, s->data());
  return strm;
}

// Declares types that can be read from an input stream.
template <class... T>
std::istream &ReadType(std::istream &strm, std::vector<T...> *c);
template <class... T>
std::istream &ReadType(std::istream &strm, std::list<T...> *c);
template <class... T>
std::istream &ReadType(std::istream &strm, std::set<T...> *c);
template <class... T>
std::istream &ReadType(std::istream &strm, std::map<T...> *c);
template <class... T>
std::istream &ReadType(std::istream &strm, std::unordered_map<T...> *c);
template <class... T>
std::istream &ReadType(std::istream &strm, std::unordered_set<T...> *c);

// Pair case.
template <typename S, typename T>
inline std::istream &ReadType(std::istream &strm, std::pair<S, T> *p) {
  ReadType(strm, &p->first);
  ReadType(strm, &p->second);
  return strm;
}

template <typename S, typename T>
inline std::istream &ReadType(std::istream &strm, std::pair<const S, T> *p) {
  ReadType(strm, const_cast<S *>(&p->first));
  ReadType(strm, &p->second);
  return strm;
}

namespace internal {
template <class C, class ReserveFn>
std::istream &ReadContainerType(std::istream &strm, C *c, ReserveFn reserve) {
  c->clear();
  int64_t n = 0;
  ReadType(strm, &n);
  reserve(c, n);
  auto insert = std::inserter(*c, c->begin());
  for (int64_t i = 0; i < n; ++i) {
    typename C::value_type value;
    ReadType(strm, &value);
    *insert = value;
  }
  return strm;
}

// Generic vector case.
template <typename T, class A,
          typename std::enable_if_t<std::is_class_v<T>, T> * = nullptr>
inline std::istream &ReadVectorType(std::istream &strm, std::vector<T, A> *c) {
  return internal::ReadContainerType(
      strm, c, [](decltype(c) v, int n) { v->reserve(n); });
}

// Vector of numerics (boolean, integral, floating-point, char) case.
template <typename T, class A,
          typename std::enable_if_t<std::is_arithmetic_v<T>, T> * = nullptr>
inline std::istream &ReadVectorType(std::istream &strm, std::vector<T, A> *c) {
  c->clear();
  int64_t n = 0;
  ReadType(strm, &n);
  if (n == 0) return strm;
  c->resize(n);
  ReadTypeN(strm, n, c->data());
  return strm;
}
}  // namespace internal

template <class T, size_t N>
std::istream &ReadType(std::istream &strm, std::array<T, N> *c) {
  if (std::is_arithmetic_v<T>) {
    ReadTypeN(strm, c->size(), c->data());
  } else {
    for (auto &v : *c) ReadType(strm, &v);
  }
  return strm;
}

template <class... T>
std::istream &ReadType(std::istream &strm, std::vector<T...> *c) {
  return internal::ReadVectorType(strm, c);
}

template <class... T>
std::istream &ReadType(std::istream &strm, std::list<T...> *c) {
  return internal::ReadContainerType(strm, c, [](decltype(c) v, int n) {});
}

template <class... T>
std::istream &ReadType(std::istream &strm, std::set<T...> *c) {
  return internal::ReadContainerType(strm, c, [](decltype(c) v, int n) {});
}

template <class... T>
std::istream &ReadType(std::istream &strm, std::map<T...> *c) {
  return internal::ReadContainerType(strm, c, [](decltype(c) v, int n) {});
}

template <class... T>
std::istream &ReadType(std::istream &strm, std::unordered_set<T...> *c) {
  return internal::ReadContainerType(
      strm, c, [](decltype(c) v, int n) { v->reserve(n); });
}

template <class... T>
std::istream &ReadType(std::istream &strm, std::unordered_map<T...> *c) {
  return internal::ReadContainerType(
      strm, c, [](decltype(c) v, int n) { v->reserve(n); });
}

// Writes types to an output stream.

// Generic case.
template <class T, typename std::enable_if<
                       std::is_class<T>::value &&
                           // `string_view` is handled separately below.
                           !std::is_convertible<T, std::string_view>::value,
                       T>::type * = nullptr>
inline std::ostream &WriteType(std::ostream &strm, const T t) {
  t.Write(strm);
  return strm;
}

// Numeric (boolean, integral, floating-point) case.
template <class T,
          typename std::enable_if_t<std::is_arithmetic_v<T>, T> * = nullptr>
inline std::ostream &WriteType(std::ostream &strm, const T t) {
  return strm.write(reinterpret_cast<const char *>(&t), sizeof(T));
}

inline std::ostream &WriteType(std::ostream &strm, std::string_view s) {
  int32_t ns = s.size();
  WriteType(strm, ns);
  return strm.write(s.data(), ns);
}

// Declares types that can be written to an output stream.

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::vector<T...> &c);

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::list<T...> &c);

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::set<T...> &c);

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::map<T...> &c);

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::unordered_map<T...> &c);

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::unordered_set<T...> &c);

// Pair case.
template <typename S, typename T>
inline std::ostream &WriteType(std::ostream &strm,
                               const std::pair<S, T> &p) {
  WriteType(strm, p.first);
  WriteType(strm, p.second);
  return strm;
}

namespace internal {
template <class C>
std::ostream &WriteSequence(std::ostream &strm, const C &c) {
  for (const auto &e : c) {
    WriteType(strm, e);
  }
  return strm;
}

template <class C>
std::ostream &WriteContainer(std::ostream &strm, const C &c) {
  const int64_t n = c.size();
  WriteType(strm, n);
  WriteSequence(strm, c);
  return strm;
}
}  // namespace internal

template <class T, size_t N>
std::ostream &WriteType(std::ostream &strm, const std::array<T, N> &c) {
  return internal::WriteSequence(strm, c);
}

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::vector<T...> &c) {
  return internal::WriteContainer(strm, c);
}

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::list<T...> &c) {
  return internal::WriteContainer(strm, c);
}

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::set<T...> &c) {
  return internal::WriteContainer(strm, c);
}

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::map<T...> &c) {
  return internal::WriteContainer(strm, c);
}

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::unordered_map<T...> &c) {
  return internal::WriteContainer(strm, c);
}

template <typename... T>
std::ostream &WriteType(std::ostream &strm, const std::unordered_set<T...> &c) {
  return internal::WriteContainer(strm, c);
}

// Utilities for converting between int64_t or Weight and string.

// Parses a 64-bit signed integer in some base out of an input string. The
// string should consist only of digits (no prefixes such as "0x") and an
// optionally preceding minus. Returns a value iff the entirety of the string is
// consumed during integer parsing, otherwise returns `std::nullopt`.
std::optional<int64_t> ParseInt64(std::string_view s, int base = 10);

int64_t StrToInt64(std::string_view s, std::string_view source, size_t nline,
                   bool allow_negative, bool *error = nullptr);

template <typename Weight>
Weight StrToWeight(std::string_view s) {
  Weight w;
  std::istringstream strm(std::string{s});
  strm >> w;
  if (!strm) {
    FSTERROR() << "StrToWeight: Bad weight: " << s;
    return Weight::NoWeight();
  }
  return w;
}

template <typename Weight>
std::string WeightToStr(Weight w) {
  std::ostringstream strm;
  strm.precision(9);
  strm << w;
  return strm.str();
}

// Utilities for reading/writing integer pairs (typically labels).

template <typename I>
bool ReadIntPairs(const std::string &source,
                  std::vector<std::pair<I, I>> *pairs,
                  bool allow_negative = false) {
  std::ifstream strm(source, std::ios_base::in);
  if (!strm) {
    LOG(ERROR) << "ReadIntPairs: Can't open file: " << source;
    return false;
  }
  const int kLineLen = 8096;
  char line[kLineLen];
  size_t nline = 0;
  pairs->clear();
  while (strm.getline(line, kLineLen)) {
    ++nline;
    std::vector<std::string_view> col =
        StrSplit(line, ByAnyChar("\n\t "), SkipEmpty());
    // empty line or comment?
    if (col.empty() || col[0].empty() || col[0][0] == '#') continue;
    if (col.size() != 2) {
      LOG(ERROR) << "ReadIntPairs: Bad number of columns, "
                 << "file = " << source << ", line = " << nline;
      return false;
    }
    bool err;
    I i1 = StrToInt64(col[0], source, nline, allow_negative, &err);
    if (err) return false;
    I i2 = StrToInt64(col[1], source, nline, allow_negative, &err);
    if (err) return false;
    pairs->emplace_back(i1, i2);
  }
  return true;
}

template <typename I>
bool WriteIntPairs(const std::string &source,
                   const std::vector<std::pair<I, I>> &pairs) {
  std::ofstream fstrm;
  if (!source.empty()) {
    fstrm.open(source);
    if (!fstrm) {
      LOG(ERROR) << "WriteIntPairs: Can't open file: " << source;
      return false;
    }
  }
  std::ostream &ostrm = fstrm.is_open() ? fstrm : std::cout;
  for (const auto &pair : pairs) {
    ostrm << pair.first << "\t" << pair.second << "\n";
  }
  return !!ostrm;
}

// Utilities for reading/writing label pairs.

template <typename Label>
bool ReadLabelPairs(const std::string &source,
                    std::vector<std::pair<Label, Label>> *pairs,
                    bool allow_negative = false) {
  return ReadIntPairs(source, pairs, allow_negative);
}

template <typename Label>
bool WriteLabelPairs(const std::string &source,
                     const std::vector<std::pair<Label, Label>> &pairs) {
  return WriteIntPairs(source, pairs);
}

// Utilities for converting a type name to a legal C symbol.

void ConvertToLegalCSymbol(std::string *s);

// Utilities for stream I/O.

bool AlignInput(std::istream &strm, size_t align = MappedFile::kArchAlignment);
bool AlignOutput(std::ostream &strm, size_t align = MappedFile::kArchAlignment);

// An associative container for which testing membership is faster than an STL
// set if members are restricted to an interval that excludes most non-members.
// A Key must have ==, !=, and < operators defined. Element NoKey should be a
// key that marks an uninitialized key and is otherwise unused. Find() returns
// an STL const_iterator to the match found, otherwise it equals End().
template <class Key, Key NoKey>
class CompactSet {
 public:
  using const_iterator = typename std::set<Key>::const_iterator;

  CompactSet() : min_key_(NoKey), max_key_(NoKey) {}

  CompactSet(const CompactSet &) = default;

  void Insert(Key key) {
    set_.insert(key);
    if (min_key_ == NoKey || key < min_key_) min_key_ = key;
    if (max_key_ == NoKey || max_key_ < key) max_key_ = key;
  }

  void Erase(Key key) {
    set_.erase(key);
    if (set_.empty()) {
      min_key_ = max_key_ = NoKey;
    } else if (key == min_key_) {
      ++min_key_;
    } else if (key == max_key_) {
      --max_key_;
    }
  }

  void Clear() {
    set_.clear();
    min_key_ = max_key_ = NoKey;
  }

  const_iterator Find(Key key) const {
    if (min_key_ == NoKey || key < min_key_ || max_key_ < key) {
      return set_.end();
    } else {
      return set_.find(key);
    }
  }

  bool Member(Key key) const {
    if (min_key_ == NoKey || key < min_key_ || max_key_ < key) {
      return false;  // out of range
    } else if (min_key_ != NoKey && max_key_ + 1 == min_key_ + set_.size()) {
      return true;  // dense range
    } else {
      return set_.count(key);
    }
  }

  const_iterator Begin() const { return set_.begin(); }

  const_iterator End() const { return set_.end(); }

  // All stored keys are greater than or equal to this value.
  Key LowerBound() const { return min_key_; }

  // All stored keys are less than or equal to this value.
  Key UpperBound() const { return max_key_; }

 private:
  std::set<Key> set_;
  Key min_key_;
  Key max_key_;

  void operator=(const CompactSet &) = delete;
};

}  // namespace fst

#endif  // FST_UTIL_H_
