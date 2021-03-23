// Copyright 2018 The Abseil Authors.
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

#ifndef IRESEARCH_ABSL_HASH_INTERNAL_SPY_HASH_STATE_H_
#define IRESEARCH_ABSL_HASH_INTERNAL_SPY_HASH_STATE_H_

#include <ostream>
#include <string>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace hash_internal {

// SpyHashState is an implementation of the HashState API that simply
// accumulates all input bytes in an internal buffer. This makes it useful
// for testing AbslHashValue overloads (so long as they are templated on the
// HashState parameter), since it can report the exact hash representation
// that the AbslHashValue overload produces.
//
// Sample usage:
// EXPECT_EQ(SpyHashState::combine(SpyHashState(), foo),
//           SpyHashState::combine(SpyHashState(), bar));
template <typename T>
class SpyHashStateImpl : public HashStateBase<SpyHashStateImpl<T>> {
 public:
  SpyHashStateImpl() : error_(std::make_shared<iresearch_absl::optional<std::string>>()) {
    static_assert(std::is_void<T>::value, "");
  }

  // Move-only
  SpyHashStateImpl(const SpyHashStateImpl&) = delete;
  SpyHashStateImpl& operator=(const SpyHashStateImpl&) = delete;

  SpyHashStateImpl(SpyHashStateImpl&& other) noexcept {
    *this = std::move(other);
  }

  SpyHashStateImpl& operator=(SpyHashStateImpl&& other) noexcept {
    hash_representation_ = std::move(other.hash_representation_);
    error_ = other.error_;
    moved_from_ = other.moved_from_;
    other.moved_from_ = true;
    return *this;
  }

  template <typename U>
  SpyHashStateImpl(SpyHashStateImpl<U>&& other) {  // NOLINT
    hash_representation_ = std::move(other.hash_representation_);
    error_ = other.error_;
    moved_from_ = other.moved_from_;
    other.moved_from_ = true;
  }

  template <typename A, typename... Args>
  static SpyHashStateImpl combine(SpyHashStateImpl s, const A& a,
                                  const Args&... args) {
    // Pass an instance of SpyHashStateImpl<A> when trying to combine `A`. This
    // allows us to test that the user only uses this instance for combine calls
    // and does not call AbslHashValue directly.
    // See AbslHashValue implementation at the bottom.
    s = SpyHashStateImpl<A>::HashStateBase::combine(std::move(s), a);
    return SpyHashStateImpl::combine(std::move(s), args...);
  }
  static SpyHashStateImpl combine(SpyHashStateImpl s) {
    if (direct_absl_hash_value_error_) {
      *s.error_ = "AbslHashValue should not be invoked directly.";
    } else if (s.moved_from_) {
      *s.error_ = "Used moved-from instance of the hash state object.";
    }
    return s;
  }

  static void SetDirectAbslHashValueError() {
    direct_absl_hash_value_error_ = true;
  }

  // Two SpyHashStateImpl objects are equal if they hold equal hash
  // representations.
  friend bool operator==(const SpyHashStateImpl& lhs,
                         const SpyHashStateImpl& rhs) {
    return lhs.hash_representation_ == rhs.hash_representation_;
  }

  friend bool operator!=(const SpyHashStateImpl& lhs,
                         const SpyHashStateImpl& rhs) {
    return !(lhs == rhs);
  }

  enum class CompareResult {
    kEqual,
    kASuffixB,
    kBSuffixA,
    kUnequal,
  };

  static CompareResult Compare(const SpyHashStateImpl& a,
                               const SpyHashStateImpl& b) {
    const std::string a_flat = iresearch_absl::StrJoin(a.hash_representation_, "");
    const std::string b_flat = iresearch_absl::StrJoin(b.hash_representation_, "");
    if (a_flat == b_flat) return CompareResult::kEqual;
    if (iresearch_absl::EndsWith(a_flat, b_flat)) return CompareResult::kBSuffixA;
    if (iresearch_absl::EndsWith(b_flat, a_flat)) return CompareResult::kASuffixB;
    return CompareResult::kUnequal;
  }

  // operator<< prints the hash representation as a hex and ASCII dump, to
  // facilitate debugging.
  friend std::ostream& operator<<(std::ostream& out,
                                  const SpyHashStateImpl& hash_state) {
    out << "[\n";
    for (auto& s : hash_state.hash_representation_) {
      size_t offset = 0;
      for (char c : s) {
        if (offset % 16 == 0) {
          out << iresearch_absl::StreamFormat("\n0x%04x: ", offset);
        }
        if (offset % 2 == 0) {
          out << " ";
        }
        out << iresearch_absl::StreamFormat("%02x", c);
        ++offset;
      }
      out << "\n";
    }
    return out << "]";
  }

  // The base case of the combine recursion, which writes raw bytes into the
  // internal buffer.
  static SpyHashStateImpl combine_contiguous(SpyHashStateImpl hash_state,
                                             const unsigned char* begin,
                                             size_t size) {
    const size_t large_chunk_stride = PiecewiseChunkSize();
    if (size > large_chunk_stride) {
      // Combining a large contiguous buffer must have the same effect as
      // doing it piecewise by the stride length, followed by the (possibly
      // empty) remainder.
      while (size >= large_chunk_stride) {
        hash_state = SpyHashStateImpl::combine_contiguous(
            std::move(hash_state), begin, large_chunk_stride);
        begin += large_chunk_stride;
        size -= large_chunk_stride;
      }
    }

    hash_state.hash_representation_.emplace_back(
        reinterpret_cast<const char*>(begin), size);
    return hash_state;
  }

  using SpyHashStateImpl::HashStateBase::combine_contiguous;

  iresearch_absl::optional<std::string> error() const {
    if (moved_from_) {
      return "Returned a moved-from instance of the hash state object.";
    }
    return *error_;
  }

 private:
  template <typename U>
  friend class SpyHashStateImpl;

  // This is true if SpyHashStateImpl<T> has been passed to a call of
  // AbslHashValue with the wrong type. This detects that the user called
  // AbslHashValue directly (because the hash state type does not match).
  static bool direct_absl_hash_value_error_;

  std::vector<std::string> hash_representation_;
  // This is a shared_ptr because we want all instances of the particular
  // SpyHashState run to share the field. This way we can set the error for
  // use-after-move and all the copies will see it.
  std::shared_ptr<iresearch_absl::optional<std::string>> error_;
  bool moved_from_ = false;
};

template <typename T>
bool SpyHashStateImpl<T>::direct_absl_hash_value_error_;

template <bool& B>
struct OdrUse {
  constexpr OdrUse() {}
  bool& b = B;
};

template <void (*)()>
struct RunOnStartup {
  static bool run;
  static constexpr OdrUse<run> kOdrUse{};
};

template <void (*f)()>
bool RunOnStartup<f>::run = (f(), true);

template <
    typename T, typename U,
    // Only trigger for when (T != U),
    typename = iresearch_absl::enable_if_t<!std::is_same<T, U>::value>,
    // This statement works in two ways:
    //  - First, it instantiates RunOnStartup and forces the initialization of
    //    `run`, which set the global variable.
    //  - Second, it triggers a SFINAE error disabling the overload to prevent
    //    compile time errors. If we didn't disable the overload we would get
    //    ambiguous overload errors, which we don't want.
    int = RunOnStartup<SpyHashStateImpl<T>::SetDirectAbslHashValueError>::run>
void AbslHashValue(SpyHashStateImpl<T>, const U&);

using SpyHashState = SpyHashStateImpl<void>;

}  // namespace hash_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_HASH_INTERNAL_SPY_HASH_STATE_H_
