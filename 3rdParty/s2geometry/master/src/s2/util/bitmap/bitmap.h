// Copyright 2000 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//
//
//
// A simple bitmap class that handles all that bit twiddling
// NOTE: You have to do your bounds checking. Especially if you
// pass in your own map without providing a size.
//

#ifndef UTIL_BITMAP_BITMAP_H__
#define UTIL_BITMAP_BITMAP_H__

#include <algorithm>
#include <climits>
#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "absl/base/macros.h"
#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/hash/hash.h"
#include "s2/util/bits/bits.h"

namespace util {
namespace bitmap {

template <typename W>
void SetBit(W* map, size_t index, bool value) {
  static constexpr size_t kIntBits = CHAR_BIT * sizeof(W);
  // This is written in such a way that our current compiler generates
  // a conditional move instead of a conditional branch, which is data
  // dependent and unpredictable.  Branch mis-prediction is much more
  // expensive than cost of a conditional move.
  const W bit = W{1} << (index & (kIntBits - 1));
  const W old_value = map[index / kIntBits];
  const W new_value = value ? old_value | bit : old_value & ~bit;
  map[index / kIntBits] = new_value;
}

template <typename W>
bool GetBit(const W* map, size_t index) {
  static constexpr size_t kIntBits = CHAR_BIT * sizeof(W);
  return map[index / kIntBits] & (W{1} << (index & (kIntBits - 1)));
}

namespace internal {
template <typename W>
class BasicBitmap {
 public:
  using size_type = size_t;
  using Word = W;  // packed bit internal storage type.

  // Allocates a new bitmap with size bits set to the value fill.
  BasicBitmap(size_type size, bool fill) : size_(size), alloc_(true) {
    map_ = std::allocator<Word>().allocate(array_size());
    SetAll(fill);
  }

  explicit BasicBitmap(size_type size) : BasicBitmap(size, false) {}

  // Creates a BasicBitmap with the given sequence of boolean values as bits.
  explicit BasicBitmap(const absl::Span<const bool> values)
      : BasicBitmap(values.size()) {
    int index = 0;
    for (bool value : values) {
      Set(index++, value);
    }
  }

  // Borrows a reference to a region of memory that is the caller's
  // responsibility to manage for the life of the Bitmap. The map is expected
  // to have enough memory to store size bits.
  BasicBitmap(Word* map, size_type size)
      : map_(map), size_(size), alloc_(false) {}

  // Default constructor: creates a bitmap with zero bits.
  BasicBitmap() : size_(0), alloc_(true) {
    map_ = std::allocator<Word>().allocate(array_size());
  }

  // Copy constructors.  The move constructor will steal the internal memory of
  // the `src` bitmap and change `src` to be a reference to the new bitmap.
  BasicBitmap(const BasicBitmap& src);
  BasicBitmap(BasicBitmap&& src);

  // Assigns this Bitmap to the values of the src Bitmap.
  // This includes pointing to the same underlying map_ if the src Bitmap
  // does not allocate its own.
  BasicBitmap& operator=(const BasicBitmap& src);

  // Destructor : clean up if we allocated
  ~BasicBitmap() {
    if (alloc_) {
      std::allocator<Word>().deallocate(map_, array_size());
    }
  }

  // Resizes the bitmap.
  // If size < bits(), the extra bits will be discarded.
  // If size > bits(), the extra bits will be filled with the fill value.
  void Resize(size_type size, bool fill = false);

  // ACCESSORS
  size_type bits() const { return size_; }
  size_type array_size() const { return RequiredArraySize(bits()); }

  // Gets an entry of the internal map. Requires array_index < array_size()
  Word GetMapElement(size_type array_index) const {
    S2_CHECK_LT(array_index, array_size());
    return map_[array_index];
  }

  // Gets an entry of the internal map. Requires array_index < array_size()
  // Also performs masking to insure no bits >= bits().
  Word GetMaskedMapElement(size_type array_index) const {
    return (array_index == array_size() - 1)
               ? map_[array_size() - 1] & HighOrderMapElementMask()
               : map_[array_index];
  }

  // Sets an element of the internal map. Requires array_index < array_size()
  void SetMapElement(size_type array_index, Word value) {
    S2_CHECK_LT(array_index, array_size());
    map_[array_index] = value;
  }

  // The highest order element in map_ will have some meaningless bits
  // (with undefined values) if bits() is not a multiple of
  // kIntBits. If you & HighOrderMapElementMask with the high order
  // element, you will be left with only the valid, defined bits (the
  // others will be 0)
  Word HighOrderMapElementMask() const {
    return (size_ == 0) ? 0 : kAllOnesWord >> (-size_ & (kIntBits - 1));
  }

  bool Get(size_type index) const {
    S2_DCHECK_LT(index, size_);
    return GetBit(map_, index);
  }

  // Returns the number of one bits in the range start to end - 1.
  // REQUIRES: start <= end <= bits()
  size_type GetOnesCountInRange(size_type start, size_type end) const;

  // Returns the number of one bits in positions 0 to limit - 1.
  // REQUIRES: limit <= bits()
  size_type GetOnesCountBeforeLimit(size_type limit) const {
    return GetOnesCountInRange(0, limit);
  }

  // Returns the number of one bits in the bitmap.
  size_type GetOnesCount() const { return GetOnesCountInRange(0, size_); }

  // Returns the number of zero bits in the range start to end - 1.
  // REQUIRES: start <= end <= bits()
  size_type GetZeroesCountInRange(size_type start, size_type end) const {
    return end - start - GetOnesCountInRange(start, end);
  }

  // Returns the number of zero bits in positions 0 to limit - 1.
  // REQUIRES: limit <= bits()
  size_type GetZeroesCountBeforeLimit(size_type limit) const {
    return limit - GetOnesCountInRange(0, limit);
  }

  // Returns the number of zero bits in the bitmap.
  size_type GetZeroesCount() const {
    return size_ - GetOnesCountInRange(0, size_);
  }

  // Returns true if all bits are unset
  bool IsAllZeroes() const {
    return std::all_of(map_, map_ + array_size() - 1,
                       [](Word w) { return w == W{0}; }) &&
           (map_[array_size() - 1] & HighOrderMapElementMask()) == W{0};
  }

  // Returns true if all bits are set
  bool IsAllOnes() const {
    return std::all_of(map_, map_ + array_size() - 1,
                       [](Word w) { return w == kAllOnesWord; }) &&
           ((~map_[array_size() - 1]) & HighOrderMapElementMask()) == W{0};
  }

  // FindNextSetBitBeforeLimit: Finds the first offset >= "*index" and
  // < "limit" that has its bit set.  If found, sets "*index" to this
  // offset and returns true.  Otherwise, does not modify "*index" and
  // returns false.  REQUIRES: "limit" <= bits().
  //
  // FindNextSetBit: like above, with implicit limit == bits().
  //
  // Note that to use these methods in a loop you must increment
  // the index after each use, as in:
  //
  //  for (Bitmap::size_type index = 0 ; map->FindNextSetBit(&index) ;
  //       ++index ) {
  //    DoSomethingWith(index);
  //  }
  //
  bool FindNextSetBitBeforeLimit(size_type* index, size_type limit) const;

  bool FindNextSetBit(size_type* index) const {
    return FindNextSetBitBeforeLimit(index, size_);
  }

  // A static version of FindNextSetBitBeforeLimit that can be called
  // by other clients that have an array of words in their hands,
  // layed out in the same way as BitMap.  Scans bits in "*words"
  // starting at bit "*bit_index", looking for a set bit.  If it finds
  // a set bit before reaching bit index "bit_limit", sets
  // "*bit_index" to the bit index and returns true.  Otherwise
  // returns false.  Will not dereference "words" past
  // "words[(bit_limit+31)/kIntBits]".
  static bool FindNextSetBitInVector(const Word* words, size_t* bit_index,
                                     size_t bit_limit) {
    return FindNextBitInVector(/*complement=*/false, words, bit_index,
                               bit_limit);
  }

  // If no bits in this bitmap are set, returns false. Otherwise returns true
  // and puts the index of the first set bit in this bitmap in *index. Note
  // that *index is modified even if we return false.
  bool FindFirstSetBit(size_type* index) const {
    *index = 0;
    return FindNextSetBit(index);
  }

  // FindNextUnsetBitBeforeLimit: Finds the first offset >= "*index" and
  // < "limit" that does NOT have its bit set.  If found, sets "*index" to this
  // offset and returns true.  Otherwise, does not modify "*index" and
  // returns false.  REQUIRES: "limit" <= bits().
  //
  // FindNextUnsetBit: like above, with implicit limit == bits().
  //
  // Note that to use these methods in a loop you must increment
  // the index after each use, as in:
  //
  //  for (Bitmap::size_type index = 0; map->FindNextUnsetBit(&index);
  //       ++index ) {
  //    DoSomethingWith(index);
  //  }
  //
  bool FindNextUnsetBitBeforeLimit(size_type* index, size_type limit) const;

  bool FindNextUnsetBit(size_type* index) const {
    return FindNextUnsetBitBeforeLimit(index, size_);
  }

  // A static version of FindNextUnsetBitBeforeLimit that can be called
  // by other clients that have an array of words in their hands,
  // layed out in the same way as BitMap.  Scans bits in "*words"
  // starting at bit "*bit_index", looking for an unset bit.  If it finds
  // an unset bit before reaching bit index "bit_limit", sets
  // "*bit_index" to the bit index and returns true.  Otherwise
  // returns false.  Will not dereference "words" past
  // "words[(bit_limit+31)/kIntBits]".
  static bool FindNextUnsetBitInVector(const Word* words, size_t* bit_index,
                                       size_t bit_limit) {
    return FindNextBitInVector(/*complement=*/true, words, bit_index,
                               bit_limit);
  }

  // If *index < bits(), finds the first offset <= "*index" and >= "limit" that
  // has its bit set.  If found, sets "*index" to this offset and returns true.
  // Otherwise, does not modify "*index" and returns false.
  //
  // Note that to use these methods in a loop you must decrement
  // the index after each use, as in:
  //
  //  for (Bitmap::size_type index = 100 ; map->FindPreviousSetBit(&index) ;
  //        --index ) {
  //    DoSomethingWith(index);
  //  }
  //
  //  Word to the wise: This depends on unsigned types (i.e. "size_type")
  //  wrapping to std::numeric_limits::max<> when decremented below zero.  This
  //  is achieved by returning false if *index >= bits().
  bool FindPreviousSetBitBeforeLimit(size_type* index, size_type limit) const;

  // Equivalent to FindPreviousSetBit with "limit" set to 0.
  bool FindPreviousSetBit(size_type* index) const {
    return FindPreviousSetBitBeforeLimit(index, 0);
  }

  // If *index < bits(), finds the first offset <= "*index" and >= "limit" that
  // does NOT have its bit set.  If found, sets "*index" to this offset and
  // returns true.  Otherwise, does not modify "*index" and returns false.
  //
  // Note that to use these methods in a loop you must decrement
  // the index after each use, as in:
  //
  //  for (Bitmap::size_type index = 100 ; map->FindPreviousUnsetBit(&index) ;
  //        --index ) {
  //    DoSomethingWith(index);
  //  }
  //
  //  Word to the wise: This depends on unsigned types (i.e. "size_type")
  //  wrapping to std::numeric_limits::max<> when decremented below zero.  This
  //  is achieved by returning false if *index >= bits().
  bool FindPreviousUnsetBitBeforeLimit(size_type* index, size_type limit) const;

  // Equivalent to FindPreviousUnsetBit with "limit" set to 0.
  bool FindPreviousUnsetBit(size_type* index) const {
    return FindPreviousUnsetBitBeforeLimit(index, 0);
  }

  // If no bits in this bitmap are set, returns false. Otherwise returns true
  // and puts the index of the first set bit in this bitmap in *index. Note
  // that *index is modified even if we return false.
  bool FindLastSetBit(size_type* index) const {
    *index = size_ - 1;  // Note that this handles size_ == 0 correctly.
    return FindPreviousSetBitBeforeLimit(index, 0);
  }

  void Set(size_type index, bool value) {
    S2_DCHECK_LT(index, size_);
    SetBit(map_, index, value);
  }

  void Toggle(size_type index) {
    S2_DCHECK_LT(index, size_);
    map_[index / kIntBits] ^= (W{1} << (index & (kIntBits - 1)));
  }

  // Sets all the bits to true or false
  void SetAll(bool value) {
    std::fill(map_, map_ + array_size(), value ? kAllOnesWord : W{0});
  }

  // Clears all bits in the bitmap
  void Clear() { SetAll(false); }

  // Sets a range of bits (begin inclusive, end exclusive) to true or false
  void SetRange(size_type begin, size_type end, bool value);

  // Sets "this" to be the union of "this" and "other". The bitmaps do
  // not have to be the same size. If other is smaller, all the higher
  // order bits are assumed to be 0. The size of "this" is never
  // changed by this operation (higher order bits in other are
  // ignored). Note this make Union *not* commutative -- it matters
  // which Bitmap is this and which is other
  void Union(const BasicBitmap& other);

  // Sets "this" to be the intersection of "this" and "other". The
  // bitmaps do not have to be the same size. If other is smaller, all
  // the higher order bits are assumed to be 0. The size of this is
  // never changed by this operation (higher order bits in other are
  // ignored)
  void Intersection(const BasicBitmap& other);

  // Returns true if "this" and "other" have any bits set in common.
  bool IsIntersectionNonEmpty(const BasicBitmap& other) const;

  // Sets "this" to be the "~" (Complement) of "this".
  void Complement() {
    std::transform(map_, map_ + array_size(), map_,
                   [](Word w) -> Word { return ~w; });
  }

  // Sets "this" to be the set of bits in "this" but not in "other"
  // REQUIRES: "bits() == other.bits()" (i.e. the bitmaps are the same size)
  void Difference(const BasicBitmap& other) {
    S2_CHECK_EQ(bits(), other.bits());
    std::transform(map_, map_ + array_size(), other.map_, map_,
                   [](Word a, Word b) { return a & ~b; });
  }

  // Sets "this" to be the set of bits which is set in either "this" or "other",
  // but not both.
  // REQUIRES: "bits() == other.bits()" (i.e. the bitmaps are the same size)
  void ExclusiveOr(const BasicBitmap& other) {
    S2_CHECK_EQ(bits(), other.bits());
    std::transform(map_, map_ + array_size(), other.map_, map_,
                   [](Word a, Word b) { return a ^ b; });
  }

  // Return true if any bit between begin inclusive and end exclusive
  // is set.  0 <= begin <= end <= bits() is required.
  bool TestRange(size_type begin, size_type end) const;

  // Return true if both Bitmaps are of equal length and have the same
  // value.
  bool IsEqual(const BasicBitmap& other) const {
    return (bits() == other.bits()) &&
           ((array_size() < 1) ||
            std::equal(map_, map_ + array_size() - 1, other.map_)) &&
           ((HighOrderMapElementMask() & other.map_[array_size() - 1]) ==
            (HighOrderMapElementMask() & map_[array_size() - 1]));
  }
  friend bool operator==(const BasicBitmap& lhs, const BasicBitmap& rhs) {
    return lhs.IsEqual(rhs);
  }
  friend bool operator!=(const BasicBitmap& lhs, const BasicBitmap& rhs) {
    return !lhs.IsEqual(rhs);
  }

  // Return true is this bitmap is a subset of another bitmap in terms of
  // the positions of 1s. That is, 0110 is a subset of 1110.
  // REQUIRES: "bits() == other.bits()" (i.e. the bitmaps are the same size)
  bool IsSubsetOf(const BasicBitmap& other) const;

  // Returns 0 if the two bitmaps are equal.  Returns a negative number if the
  // this bitmap is less than other, and a positive number otherwise.
  //
  // The relation we use is the natural relation defined by assigning an integer
  // to each bitmap:
  //
  // int(bitmap) = b_0 + 2 * b_1 + ... + 2^k * b_k
  //
  // Then for our comparison function:
  //
  // if int(b1) != int(b2), then b1 is less than b2 if int(b1) < int(b2),
  // and b2 is less than b1 otherwise.
  //
  // if int(b1) == int(b2), then we compare the numbers of bits in b1 and b2.
  // If b1 has strictly fewer bits, then b1 is less than b2 (same for b2).
  // If b1 and b2 have the same number of bits, then they are equal and we
  // return 0.
  int CompareTo(const BasicBitmap& other) const;
  friend bool operator<(const BasicBitmap& lhs, const BasicBitmap& rhs) {
    return lhs.CompareTo(rhs) < 0;
  }
  friend bool operator<=(const BasicBitmap& lhs, const BasicBitmap& rhs) {
    return lhs.CompareTo(rhs) <= 0;
  }
  friend bool operator>(const BasicBitmap& lhs, const BasicBitmap& rhs) {
    return lhs.CompareTo(rhs) > 0;
  }
  friend bool operator>=(const BasicBitmap& lhs, const BasicBitmap& rhs) {
    return lhs.CompareTo(rhs) >= 0;
  }

  // Return the bitmap as a string of 0's and 1's in order from
  // bit 0 to the highest bit.  If <group> is specified and greater
  // than zero, write an underscore every <group> bits.
  std::string ToString() const {
    std::string str(size_, '0');
    for (size_type index = 0; FindNextSetBit(&index); ++index) str[index] = '1';
    return str;
  }
  // Return the bitmap as a string of 0's and 1's in order from bit 0 to the
  // highest bit.  If <group> is greater than zero, write an underscore every
  // <group> bits.
  std::string ToString(int group) const {
    const std::string source = ToString();
    if (group <= 0) return source;
    std::string target;
    for (size_t i = 0; i < source.size(); i += group) {
      target.append(i > 0, '_').append(source, i, group);
    }
    return target;
  }

  // Returns an efficient hash of the bitmap.
  // The algorithm used is subject to change.
  size_t HashCode() const { return absl::Hash<BasicBitmap>{}(*this); }
  template <typename H>
  friend H AbslHashValue(H state, const BasicBitmap& bitmap) {
    const size_t complete_map_entries = bitmap.size_ / bitmap.kIntBits;
    state = H::combine_contiguous(std::move(state), bitmap.map_,
                                  complete_map_entries);

    if (complete_map_entries != bitmap.array_size()) {
      const Word last_map_entry = bitmap.map_[bitmap.array_size() - 1] &
                                  bitmap.HighOrderMapElementMask();
      state = H::combine(std::move(state), last_map_entry);
    }

    return state;
  }

  // return number of allocated words required for a bitmap of size num_bits
  // minimum size is 1
  static constexpr size_t RequiredArraySize(size_type num_bits) {
    return num_bits == 0 ? 1 : (num_bits - 1) / kIntBits + 1;
  }

  class BitIndexIter {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = size_type;
    using difference_type = std::ptrdiff_t;
    using pointer = const size_type*;
    using reference = const size_type&;

    BitIndexIter() : BitIndexIter(nullptr, 0) {}

    explicit BitIndexIter(const BasicBitmap* bitmap)
        : BitIndexIter(bitmap, 0) {}

    BitIndexIter(const BasicBitmap* bitmap, size_type start_bit);

    reference operator*() const { return bit_position_; }

    pointer operator->() const { return &bit_position_; }

    BitIndexIter& operator++();

    BitIndexIter operator++(int) {
      BitIndexIter rv = *this;
      ++(*this);
      return rv;
    }

    // Lazy initialization of current_, updates bit_position_ to current bit.
    size_type BitIndex() const;

    friend bool operator==(const BitIndexIter& a, const BitIndexIter& b) {
      return a.bit_position_ == b.bit_position_;
    }

    friend bool operator!=(const BitIndexIter& a, const BitIndexIter& b) {
      return !(a == b);
    }

   private:
    const BasicBitmap* bitmap_;  // Not owned.

    // This will be 0 if and only if we are at the end, otherwise it will be
    // a cached copy of the bitmap of the word from the bit array containing
    // the bit specified by bit_position_, but logically right shifted so that
    // the current bit is at position 0.
    W current_;
    size_type bit_position_;
  };

  class BitIndexRange {
   public:
    using value_type = size_type;
    using const_iterator = BitIndexIter;

    explicit BitIndexRange(const BasicBitmap* bitmap) : bitmap_(bitmap) {}

    BitIndexIter begin() const { return LowerBound(0); }

    BitIndexIter end() const {
      return LowerBound(std::numeric_limits<value_type>::max());
    }

    // Starts the iterator from the first non-zero bit above
    // bit_index_lower_bound, if any.
    BitIndexIter LowerBound(size_t bit_index_lower_bound) const {
      return const_iterator(bitmap_, bit_index_lower_bound);
    }

   private:
    const BasicBitmap* bitmap_;
  };

  // Returns a class that can be used in for range contexts to
  // iterate through the ordinal set of indices of bits that are set,
  // for example:
  //   Bitmap bitmap(10);
  //   bitmap.Set(3);
  //   bitmap.Set(4);
  //   bitmap.Set(9);
  //   for (size_t index : bitmap.TrueBitIndices()) {
  //     std::cout << index << std::endl;
  //   }
  // would print
  //   3
  //   4
  //   9
  // The range, while valid, will always be in the half-open interval
  // [0, bitmap.bits()).
  BitIndexRange TrueBitIndices() const { return BitIndexRange(this); }

 private:
  // An unsigned integral type that is not promoted and can represent all values
  // of `Word` (assuming `Word` is unsigned). Note that if `Word` does not get
  // promoted, then this type is the same as `Word`. We occaisionally use this
  // type to perform arithmetic, to avoid having `Word`-typed values promoted to
  // a signed type via integral promotions.
  // TODO(b/228178585)
  using ArithmeticWord = absl::make_unsigned_t<decltype(+std::declval<Word>())>;

  // A value of type `Word` with all bits set to one. If `Word` !=
  // `ArithmeticWord`, i.e. `Word` gets promoted, then this is a `Word` with all
  // bits set to one then promoted to `ArithmeticWord`. In other words, the
  // lowest N bits are one, and all other bits are zero, where N is the width of
  // `Word`.
  //
  // For example, for uint32, this is 0xFFFFFFFF. However, if `Word` is
  // `unit8_t`, and `ArithmeticWord` is equal to `uint32` and kAllOnesWord is
  // 0x000000FF.
  static constexpr auto kAllOnesWord =
      ArithmeticWord{static_cast<Word>(~ArithmeticWord{0})};

  // Implements FindNextSetBitInVector if 'complement' is false,
  // and FindNextUnsetBitInVector if 'complement' is true.
  static bool FindNextBitInVector(bool complement, const Word* words,
                                  size_t* bit_index_inout, size_t limit);

  static bool FindPreviousBitInVector(bool complement, const Word* words,
                                      size_t* bit_index_inout, size_t limit);

  // The same semantics as CompareTo, except that we have the invariant that
  // first has at least as many bits as second.
  static int CompareToHelper(const BasicBitmap& first,
                             const BasicBitmap& second);

  static constexpr unsigned Log2(unsigned n, unsigned p = 0) {
    return (n <= 1) ? p : Log2(n / 2, p + 1);
  }

  // NOTE: we make assumptions throughout the code that kIntBits is a power of
  // 2, so that we can use shift and mask instead of division and modulo.
  static constexpr int kIntBits = CHAR_BIT * sizeof(Word);  // bits in a Word
  static constexpr int kLogIntBits = Log2(kIntBits, 0);
  Word* map_;       // the bitmap
  size_type size_;  // the upper bound of the bitmap
  bool alloc_;      // whether or not *we* allocated the memory
};

// Streams `bitmap.ToString()` to `out`.
template <typename W>
inline std::ostream& operator<<(std::ostream& out,
                                const BasicBitmap<W>& bitmap) {
  return out << bitmap.ToString();
}

}  // namespace internal
}  // namespace bitmap
}  // namespace util

namespace util {
namespace bitmap {

using Bitmap8 = ::util::bitmap::internal::BasicBitmap<uint8>;
using Bitmap16 = ::util::bitmap::internal::BasicBitmap<uint16>;
using Bitmap32 = ::util::bitmap::internal::BasicBitmap<uint32>;
using Bitmap64 = ::util::bitmap::internal::BasicBitmap<uint64>;

}  // namespace bitmap
}  // namespace util

// Legacy definition, use Bitmap32/Bitmap64 instead in new code.    Do not use
// this to instantiate the BasicBitmap template (i.e. do not spell
// `::Bitmap::BasicBitmap<Y>`).  Instead, use one of the aliases above (e.g.
// ::util::bitmap::Bitmap32).
// TODO(b/145388656): remove this class, once all forward declarations are gone.
class ABSL_DEPRECATED(
    "Legacy definition, use util::bitmap::{Bitmap32|Bitmap64} instead in new "
    "code.") Bitmap : public util::bitmap::internal::BasicBitmap<uint32> {
 public:
  using util::bitmap::internal::BasicBitmap<uint32>::BasicBitmap;
};

// Implementations follow.

namespace util {
namespace bitmap {
namespace internal {

template <typename W>
BasicBitmap<W>::BasicBitmap(const BasicBitmap& src)
    : size_(src.size_), alloc_(src.alloc_) {
  static_assert((kIntBits & (kIntBits - 1)) == 0, "kIntBits_not_power_of_2");
  if (alloc_) {
    map_ = std::allocator<Word>().allocate(array_size());
    std::copy(src.map_, src.map_ + array_size(), map_);
  } else {
    map_ = src.map_;
  }
}

template <typename W>
BasicBitmap<W>::BasicBitmap(BasicBitmap&& src)
    : size_(src.size_), alloc_(src.alloc_) {
  map_ = src.map_;
  if (alloc_) {
    src.alloc_ = false;
  }
}

template <typename W>
void BasicBitmap<W>::Resize(size_type size, bool fill) {
  const size_type old_size = size_;
  const size_t new_array_size = RequiredArraySize(size);
  if (new_array_size != array_size()) {
    Word* new_map = std::allocator<Word>().allocate(new_array_size);
    std::copy(map_, map_ + std::min<size_t>(new_array_size, array_size()),
              new_map);
    if (alloc_) {
      std::allocator<Word>().deallocate(map_, array_size());
    }
    map_ = new_map;
    alloc_ = true;
  }
  size_ = size;
  if (old_size < size_) {
    SetRange(old_size, size_, fill);
  }
}

template <typename W>
BasicBitmap<W>& BasicBitmap<W>::operator=(const BasicBitmap<W>& src) {
  if (this != &src) {
    if (alloc_ && array_size() != src.array_size()) {
      std::allocator<Word>().deallocate(map_, array_size());
      map_ = std::allocator<Word>().allocate(src.array_size());
    }
    size_ = src.size_;
    if (src.alloc_) {
      if (!alloc_) {
        map_ = std::allocator<Word>().allocate(src.array_size());
      }
      std::copy(src.map_, src.map_ + src.array_size(), map_);
      alloc_ = true;
    } else {
      if (alloc_) {
        std::allocator<Word>().deallocate(map_, array_size());
      }
      map_ = src.map_;
      alloc_ = false;
    }
  }
  return *this;
}

// Return true if any bit between begin inclusive and end exclusive
// is set.  0 <= begin <= end <= bits() is required.
template <typename W>
bool BasicBitmap<W>::TestRange(size_type begin, size_type end) const {
  // Return false immediately if the range is empty.
  if (begin == end) {
    return false;
  }
  // Calculate the indices of the words containing the first and last bits,
  // along with the positions of the bits within those words.
  size_t i = begin / kIntBits;
  size_t j = begin & (kIntBits - 1);
  size_t ilast = (end - 1) / kIntBits;
  size_t jlast = (end - 1) & (kIntBits - 1);
  // If the range spans multiple words, discard the extraneous bits of the
  // first word by shifting to the right, and then test the remaining bits.
  if (i < ilast) {
    if (map_[i++] >> j) {
      return true;
    }
    j = 0;

    // Test each of the "middle" words that lies completely within the range.
    while (i < ilast) {
      if (map_[i++]) {
        return true;
      }
    }
  }

  // Test the portion of the last word that lies within the range. (This logic
  // also handles the case where the entire range lies within a single word.)
  const Word mask = (((W{1} << 1) << (jlast - j)) - 1) << j;
  return (map_[ilast] & mask) != W{0};
}

template <typename W>
bool BasicBitmap<W>::IsSubsetOf(const BasicBitmap& other) const {
  S2_CHECK_EQ(bits(), other.bits());
  Word* mp = map_;
  Word* endp = mp + array_size() - 1;
  Word* op = other.map_;
  // A is a subset of B if A - B = {}, that is A & ~B = {}
  for (; mp != endp; ++mp, ++op)
    if (*mp & ~*op) return false;
  return (*mp & ~*op & HighOrderMapElementMask()) == W{0};
}

// Same semantics as CompareTo, except that we have the invariant that first
// has at least as many bits as second.
template <typename W>
int BasicBitmap<W>::CompareToHelper(const BasicBitmap<W>& first,
                                    const BasicBitmap<W>& second) {
  // Checks if the high order bits in first that are not in second are set.  If
  // any of these are set, then first is greater than second, and we return a
  // positive value.
  if (first.TestRange(second.bits(), first.bits())) {
    return 1;
  }

  // We use unsigned integer comparison to compare the bitmaps.  We need to
  // handle the high order bits in a special case (since there may be undefined
  // bits for the element representing the highest order bits) and then we
  // can do direct integer comparison.
  size_t index = second.array_size() - 1;
  Word left = first.map_[index] & second.HighOrderMapElementMask();
  Word right = second.map_[index] & second.HighOrderMapElementMask();
  if (left != right) {
    return left < right ? -1 : 1;
  }
  while (index > 0) {
    --index;
    left = first.map_[index];
    right = second.map_[index];
    if (left != right) {
      return left < right ? -1 : 1;
    }
  }
  // Now we have reached the end, all common bits are equal, and all bits that
  // are only in the longer list are 0.  We return 1 if the first bitmap is
  // strictly larger, and 0 if the bitmaps are of equal size.
  if (first.bits() == second.bits()) {
    return 0;
  } else {
    return 1;
  }
}

template <typename W>
int BasicBitmap<W>::CompareTo(const BasicBitmap<W>& other) const {
  if (bits() > other.bits()) {
    return CompareToHelper(*this, other);
  } else {
    return -CompareToHelper(other, *this);
  }
}

// Note that bits > size end up in undefined states when sizes
// aren't equal, but that's okay.
template <typename W>
void BasicBitmap<W>::Union(const BasicBitmap<W>& other) {
  const size_t this_array_size = array_size();
  const size_t other_array_size = other.array_size();
  const size_t min_array_size = std::min(this_array_size, other_array_size);
  if (min_array_size == 0) {
    // Nothing to do.
    return;
  }
  // Perform bitwise OR of all but the last common word.
  const size_t last = min_array_size - 1;
  std::transform(map_, map_ + last, other.map_, map_,
                 [](Word a, Word b) { return a | b; });
  // Perform bitwise OR of the last common word, applying mask if necessary.
  map_[last] |= other_array_size == min_array_size
                    ? other.map_[last] & other.HighOrderMapElementMask()
                    : other.map_[last];
}

// Note that bits > size end up in undefined states when sizes
// aren't equal, but that's okay.
template <typename W>
void BasicBitmap<W>::Intersection(const BasicBitmap<W>& other) {
  const size_t this_array_size = array_size();
  const size_t other_array_size = other.array_size();
  const size_t min_array_size = std::min(this_array_size, other_array_size);
  // Perform bitwise AND of all common words.
  std::transform(map_, map_ + min_array_size, other.map_, map_,
                 [](Word a, Word b) { return a & b; });
  if (other_array_size == min_array_size) {
    // Zero out bits that are outside the range of 'other'.
    if (other_array_size != 0) {
      map_[other_array_size - 1] &= other.HighOrderMapElementMask();
    }
    std::fill(map_ + other_array_size, map_ + this_array_size, 0);
  }
}

template <typename W>
bool BasicBitmap<W>::IsIntersectionNonEmpty(const BasicBitmap<W>& other) const {
  // First check fully overlapping bytes.
  size_t max_overlap = std::min(array_size(), other.array_size()) - 1;
  for (size_t i = 0; i < max_overlap; ++i) {
    if (map_[i] & other.map_[i]) return true;
  }

  // Now check the highest overlapping byte, applying bit masks as necessary.
  Word high_byte = map_[max_overlap] & other.map_[max_overlap];

  if (other.array_size() > array_size())
    return high_byte & HighOrderMapElementMask();
  else if (array_size() > other.array_size())
    return high_byte & other.HighOrderMapElementMask();

  // Same array_size, apply both masks.
  return high_byte & HighOrderMapElementMask() &
         other.HighOrderMapElementMask();
}

template <typename W>
typename BasicBitmap<W>::size_type BasicBitmap<W>::GetOnesCountInRange(
    size_type start, size_type end) const {
  S2_CHECK_LE(end, size_);
  S2_CHECK_LE(start, end);

  if (start >= end) {
    return 0;
  }  // Need this?

  size_t start_word = start / kIntBits;
  size_t end_word = (end - 1) / kIntBits;  // Word containing the last bit.

  Word* p = map_ + start_word;
  ArithmeticWord c = static_cast<ArithmeticWord>(*p) &
                     (kAllOnesWord << (start & (kIntBits - 1)));

  ArithmeticWord endmask = (kAllOnesWord >> ((end_word + 1) * kIntBits - end));
  if (end_word == start_word) {  // Only one word?
    return absl::popcount(static_cast<Word>(c & endmask));
  }

  size_type sum = absl::popcount(static_cast<Word>(c));

  for (++p; p < map_ + end_word; ++p) {
    sum += absl::popcount(*p);
  }

  return sum + absl::popcount(static_cast<Word>(
                   static_cast<ArithmeticWord>(*p) & endmask));
}

template <typename W>
bool BasicBitmap<W>::FindNextSetBitBeforeLimit(size_type* index,
                                               size_type limit) const {
  S2_CHECK_LE(limit, size_);
  size_t index_as_size_t = *index;
  bool result = FindNextSetBitInVector(map_, &index_as_size_t, limit);
  if (result) {
    *index = static_cast<size_t>(index_as_size_t);
  }
  return result;
}

template <typename W>
bool BasicBitmap<W>::FindNextUnsetBitBeforeLimit(size_type* index,
                                                 size_type limit) const {
  S2_CHECK_LE(limit, size_);
  size_t index_as_size_t = *index;
  bool result = FindNextUnsetBitInVector(map_, &index_as_size_t, limit);
  if (result) {
    *index = static_cast<size_t>(index_as_size_t);
  }
  return result;
}

/*static*/
template <typename W>
bool BasicBitmap<W>::FindNextBitInVector(bool complement, const Word* words,
                                         size_t* bit_index_inout,
                                         size_t limit) {
  const size_t bit_index = *bit_index_inout;
  if (bit_index >= limit) return false;
  // From now on limit != 0, since if it was we would have returned false.
  size_t int_index = bit_index >> kLogIntBits;
  Word one_word = words[int_index];
  if (complement) one_word = ~one_word;

  // Simple optimization where we can immediately return true if the first
  // bit is set.  This helps for cases where many bits are set, and doesn't
  // hurt too much if not.
  const size_t first_bit_offset = bit_index & (kIntBits - 1);
  if (one_word & (W{1} << first_bit_offset)) return true;

  // First word is special - we need to mask off leading bits
  one_word &= (kAllOnesWord << first_bit_offset);

  // Loop through all but the last word.  Note that 'limit' is one
  // past the last bit we want to check, and we don't want to read
  // past the end of "words".  E.g. if size_ == kIntBits only words[0] is
  // valid, so we want to avoid reading words[1] when limit == kIntBits.
  const size_t last_int_index = (limit - 1) >> kLogIntBits;
  while (int_index < last_int_index) {
    if (one_word != W{0}) {
      *bit_index_inout =
          (int_index << kLogIntBits) + Bits::FindLSBSetNonZero64(one_word);
      return true;
    }
    one_word = words[++int_index];
    if (complement) one_word = ~one_word;
  }

  // Last word is special - we may need to mask off trailing bits.  Note that
  // 'limit' is one past the last bit we want to check, and if limit is a
  // multiple of kIntBits we want to check all bits in this word.
  one_word &= ~((kAllOnesWord - 1) << ((limit - 1) & (kIntBits - 1)));
  if (one_word != 0) {
    *bit_index_inout =
        (int_index << kLogIntBits) + Bits::FindLSBSetNonZero64(one_word);
    return true;
  }
  return false;
}

/*static*/
template <typename W>
bool BasicBitmap<W>::FindPreviousBitInVector(bool complement, const Word* words,
                                             size_t* bit_index_inout,
                                             size_t limit) {
  const size_type bit_index = *bit_index_inout;
  size_t map_index = bit_index >> kLogIntBits;
  const size_t map_limit = limit >> kLogIntBits;
  const size_t bit_limit_mask = kAllOnesWord << (limit & (kIntBits - 1));
  Word one_word = complement ? ~words[map_index] : words[map_index];

  if (limit > *bit_index_inout) return false;
  // Simple optimization where we can immediately return true if the first
  // bit is set.  This helps for cases where many bits are set, and doesn't
  // hurt too much if not.
  const unsigned bit_offset = bit_index & (kIntBits - 1);
  if (one_word & (W{1} << bit_offset)) return true;

  // First word is special - we need to mask off trailing bits
  one_word &= ~((kAllOnesWord - 1) << bit_offset);
  // Then any leading bits if the limit is within this word
  if (map_index == map_limit) one_word &= bit_limit_mask;

  // Loop through all empty words
  while (one_word == 0) {
    if (map_index == map_limit) return false;
    --map_index;
    one_word = complement ? ~words[map_index] : words[map_index];
    if (map_index == map_limit) one_word &= bit_limit_mask;
  }

  // Found a word with some set bits - return result
  *bit_index_inout =
      (map_index << kLogIntBits) + Bits::FindMSBSetNonZero64(one_word);
  return true;
}

template <typename W>
bool BasicBitmap<W>::FindPreviousSetBitBeforeLimit(size_type* index,
                                                   size_type limit) const {
  if (*index >= size_) return false;
  size_t index_as_size_t = *index;
  bool result = FindPreviousBitInVector(/*complement=*/false, map_,
                                        &index_as_size_t, limit);
  if (result) {
    *index = static_cast<size_t>(index_as_size_t);
  }
  return result;
}

template <typename W>
bool BasicBitmap<W>::FindPreviousUnsetBitBeforeLimit(size_type* index,
                                                     size_type limit) const {
  if (*index >= size_) return false;
  size_t index_as_size_t = *index;
  bool result = FindPreviousBitInVector(/*complement=*/true, map_,
                                        &index_as_size_t, limit);
  if (result) {
    *index = static_cast<size_t>(index_as_size_t);
  }
  return result;
}

template <typename W>
void BasicBitmap<W>::SetRange(size_type begin, size_type end, bool value) {
  if (begin == end) return;
  // Figure out which element(s) in the map_ array are affected
  // by this op.
  const size_type begin_element = begin / kIntBits;
  const size_type begin_bit = begin % kIntBits;
  const size_type end_element = end / kIntBits;
  const size_type end_bit = end % kIntBits;
  Word initial_mask = kAllOnesWord << begin_bit;
  if (end_element == begin_element) {
    // The range is contained in a single element of the array, so
    // adjust both ends of the mask.
    initial_mask = initial_mask & (kAllOnesWord >> (kIntBits - end_bit));
  }
  if (value) {
    map_[begin_element] |= initial_mask;
  } else {
    map_[begin_element] &= ~initial_mask;
  }
  if (end_element != begin_element) {
    // Set all the bits in the array elements between the begin
    // and end elements.
    std::fill(map_ + begin_element + 1, map_ + end_element,
              value ? kAllOnesWord : W{0});

    // Update the appropriate bit-range in the last element.
    // Note end_bit is an exclusive bound, so if it's 0 none of the
    // bits in end_element are contained in the range (and we don't
    // have to modify it).
    if (end_bit != 0) {
      const Word final_mask = kAllOnesWord >> (kIntBits - end_bit);
      if (value) {
        map_[end_element] |= final_mask;
      } else {
        map_[end_element] &= ~final_mask;
      }
    }
  }
}

template <typename W>
auto BasicBitmap<W>::BitIndexIter::operator++() -> BitIndexIter& {
  // Shift away the previous bit.
  current_ >>= 1;
  ++bit_position_;
  if (current_ == 0) {
    if (bitmap_->FindNextSetBit(&bit_position_)) {
      current_ = bitmap_->GetMaskedMapElement(bit_position_ / kIntBits) >>
                 (bit_position_ & (kIntBits - 1));
    } else {
      bit_position_ = bitmap_->bits();
    }
    return *this;
  }
  // If the bottom bit is zero, shift until we find a set bit.
  if ((current_ & 1) == 0) {
    const int first = Bits::FindLSBSetNonZero64(current_);
    bit_position_ += first;
    current_ >>= first;
  }
  return *this;
}

template <typename W>
BasicBitmap<W>::BitIndexIter::BitIndexIter(const BasicBitmap* bitmap,
                                           size_type start_bit)
    : bitmap_(bitmap), current_(0), bit_position_(start_bit) {
  if (!bitmap_) {
    bit_position_ = 0;
    return;
  }
  if (!bitmap_->FindNextSetBit(&bit_position_)) {
    bit_position_ = bitmap_->bits();
    return;
  }
  current_ = bitmap_->GetMaskedMapElement(bit_position_ / kIntBits) >>
             (bit_position_ & (kIntBits - 1));
}

}  // namespace internal
}  // namespace bitmap
}  // namespace util

#endif  // UTIL_BITMAP_BITMAP_H__
