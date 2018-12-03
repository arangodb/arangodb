// Copyright 2007 Google Inc. All Rights Reserved.
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
// A btree implementation of the STL set and map interfaces. A btree is smaller
// and generally also faster than STL set/map (refer to the benchmarks below).
// The red-black tree implementation of STL set/map has an overhead of 3
// pointers (left, right and parent) plus the node color information for each
// stored value. So a set<int32> consumes 40 bytes for each value stored in
// 64-bit mode. This btree implementation stores multiple values on fixed
// size nodes (usually 256 bytes) and doesn't store child pointers for leaf
// nodes. The result is that a btree_set<int32> may use much less memory per
// stored value. For the random insertion benchmark in btree_bench.cc, a
// btree_set<int32> with node-size of 256 uses 5.1 bytes per stored value.
//
// The packing of multiple values on to each node of a btree has another effect
// besides better space utilization: better cache locality due to fewer cache
// lines being accessed. Better cache locality translates into faster
// operations.
//
// CAVEATS
//
// Insertions and deletions on a btree can cause splitting, merging or
// rebalancing of btree nodes. And even without these operations, insertions
// and deletions on a btree will move values around within a node. In both
// cases, the result is that insertions and deletions can invalidate iterators
// pointing to values other than the one being inserted/deleted. Therefore, this
// container does not provide pointer stability. This is notably different from
// STL set/map which takes care to not invalidate iterators on insert/erase
// except, of course, for iterators pointing to the value being erased.  A
// partial workaround when erasing is available: erase() returns an iterator
// pointing to the item just after the one that was erased (or end() if none
// exists).

// PERFORMANCE
//
// See the latest benchmark results at:
// https://paste.googleplex.com/5876428872613888
//

#ifndef S2_UTIL_GTL_BTREE_H_
#define S2_UTIL_GTL_BTREE_H_

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/container/internal/compressed_tuple.h"
#include "s2/third_party/absl/container/internal/container_memory.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/meta/type_traits.h"
#include "s2/third_party/absl/strings/string_view.h"
#include "s2/third_party/absl/utility/utility.h"
#include "s2/util/gtl/layout.h"

namespace gtl {

// A helper type used to indicate that a key-compare-to functor has been
// provided. A key-compare-to functor compares two arguments of type value_type
// and returns 0 if they are equal, a negative integer when the first argument
// should be first, and a positive integer otherwise. A user can specify a
// key-compare-to functor by doing:
//
//  struct MyStringComparer
//      : public gtl::btree_key_compare_to_tag {
//    int operator()(const string &a, const string &b) const {
//      return a.compare(b);
//    }
//  };
//
// Note that the return type is an int and not a bool. There is a
// static_assert which enforces this return type.
// TODO(user): get rid of this tag and just detect whether there is an operator
struct btree_key_compare_to_tag {};

// A helper class that indicates if the Compare parameter is derived from
// btree_key_compare_to_tag.
template <typename Compare>
using btree_is_key_compare_to =
    std::is_convertible<Compare, btree_key_compare_to_tag>;

namespace internal_btree {
// A helper class used to indicate if the comparator provided is transparent
// and thus supports heterogeneous lookups. This is only used internally to
// check if the Compare parameter has a valid is_transparent member.
// A transparent comparator will see lookup keys with any type (lookup_type)
// passed by the user to any of the lookup methods. The comparator then has a
// chance to do the comparison without first converting the lookup key to a
// key_type.
//
// For example, a comparator that is transparent may look like:
//
//  struct MyStringComparer {
//    bool operator()(const string &a, const string &b) const {
//      return a < b;
//    }
//    bool operator()(const string &a, const char* b) const {
//      return strcmp(a.c_str(), b) < 0;
//    }
//    bool operator()(const char* a, const string& b) const {
//      return strcmp(a, b.c_str()) < 0;
//    }
//    using is_transparent = void;
//  };
//
// Note that we need to declare operator() for both combinations of key_type and
// lookup_type. Also note that setting is_transparent to void is an arbitrary
// decision; it can be std::true_type, int, or anything else, just as long as
// the member is_transparent is defined to be something.
template <typename, typename = void>
struct is_comparator_transparent : std::false_type {};
template <typename Compare>
struct is_comparator_transparent<Compare,
                                 absl::void_t<typename Compare::is_transparent>>
    : std::true_type {};

// A helper class to convert a boolean comparison into a three-way "compare-to"
// comparison that returns a negative value to indicate less-than, zero to
// indicate equality and a positive value to indicate greater-than. This helper
// class is specialized for less<string>, greater<string>, less<string_view>,
// and greater<string_view>. The
// key_compare_to_adapter is provided so that btree users
// automatically get the more efficient compare-to code when using common
// google string types with common comparison functors.
// TODO(user): see if we can extract this logic so that it can be used with
template <typename Compare>
struct key_compare_to_adapter {
  using type = Compare;
};

template <>
struct key_compare_to_adapter<std::less<std::string>> {
  struct type : public btree_key_compare_to_tag {
    type() = default;
    explicit type(const std::less<std::string> &) {}
    int operator()(const std::string &a, const std::string &b) const {
      return a.compare(b);
    }
  };
};

template <>
struct key_compare_to_adapter<std::greater<std::string>> {
  struct type : public btree_key_compare_to_tag {
    type() = default;
    explicit type(const std::greater<std::string> &) {}
    int operator()(const std::string &a, const std::string &b) const {
      return b.compare(a);
    }
  };
};

template <>
struct key_compare_to_adapter<std::less<absl::string_view>> {
  struct type : public btree_key_compare_to_tag {
    type() = default;
    explicit type(const std::less<absl::string_view> &) {}
    int operator()(const absl::string_view a, const absl::string_view b) const {
      return a.compare(b);
    }
  };
};

template <>
struct key_compare_to_adapter<std::greater<absl::string_view>> {
  struct type : public btree_key_compare_to_tag {
    type() = default;
    explicit type(const std::greater<absl::string_view> &) {}
    int operator()(const absl::string_view a, const absl::string_view b) const {
      return b.compare(a);
    }
  };
};


// A helper function to do a boolean comparison of two keys given a boolean
// or key-compare-to (three-way) comparator.
template <typename K, typename LK, typename Compare>
bool bool_compare_keys(const Compare &comp, const K &x, const LK &y) {
  return btree_is_key_compare_to<Compare>::value ? comp(x, y) < 0 : comp(x, y);
}

// Detects a 'goog_btree_prefer_linear_node_search' member. This is
// a protocol used as an opt-in or opt-out of linear search.
//
//  For example, this would be useful for key types that wrap an integer
//  and define their own cheap operator<(). For example:
//
//   class K {
//    public:
//     using goog_btree_prefer_linear_node_search = std::true_type;
//     ...
//    private:
//     friend bool operator<(K a, K b) { return a.k_ < b.k_; }
//     int k_;
//   };
//
//   btree_map<K, V> m;  // Uses linear search
//   assert((btree_map<K, V>::testonly_uses_linear_node_search()));
//
// If T has the preference tag, then it has a preference.
// Btree will use the tag's truth value.
template <typename T, typename = void>
struct has_linear_node_search_preference : std::false_type {};
template <typename T, typename = void>
struct prefers_linear_node_search : std::false_type {};
template <typename T>
struct has_linear_node_search_preference<
    T, absl::void_t<typename T::goog_btree_prefer_linear_node_search>>
    : std::true_type {};
template <typename T>
struct prefers_linear_node_search<
    T, absl::void_t<typename T::goog_btree_prefer_linear_node_search>>
    : T::goog_btree_prefer_linear_node_search {};

template <typename Key, typename Compare, typename Alloc, int TargetNodeSize,
          int ValueSize, bool Multi>
struct common_params {
  // If Compare is derived from btree_key_compare_to_tag then use it as the
  // key_compare type. Otherwise, use key_compare_to_adapter<> which will
  // fall-back to Compare if we don't have an appropriate specialization.
  using key_compare =
      absl::conditional_t<btree_is_key_compare_to<Compare>::value, Compare,
                          typename key_compare_to_adapter<Compare>::type>;
  // A type which indicates if we have a key-compare-to functor or a plain old
  // key-compare functor.
  using is_key_compare_to = btree_is_key_compare_to<key_compare>;

  using allocator_type = Alloc;
  using key_type = Key;
  using size_type = std::make_signed<size_t>::type;
  using difference_type = ptrdiff_t;

  // True if this is a multiset or multimap.
  using is_multi_container = std::integral_constant<bool, Multi>;

  enum {
    kTargetNodeSize = TargetNodeSize,

    // Upper bound for the available space for values. This is largest for leaf
    // nodes, which have overhead of at least a pointer + 4 bytes (for storing
    // 3 field_types and an enum).
    kNodeValueSpace =
        TargetNodeSize - /*minimum overhead=*/(sizeof(void *) + 4),
  };

  // This is an integral type large enough to hold as many
  // ValueSize-values as will fit a node of TargetNodeSize bytes.
  using node_count_type =
      absl::conditional_t<kNodeValueSpace / ValueSize >= 256, uint16, uint8>;
  static_assert(kNodeValueSpace / ValueSize <=
                    std::numeric_limits<uint16>::max(),
                "uint16 is not big enough for node_count_type.");
};

// A parameters structure for holding the type parameters for a btree_map.
// Compare and Alloc should be nothrow copy-constructible.
template <typename Key, typename Data, typename Compare, typename Alloc,
          int TargetNodeSize, bool Multi>
struct map_params : common_params<Key, Compare, Alloc, TargetNodeSize,
                                  sizeof(std::pair<const Key, Data>), Multi> {
  using mapped_type = Data;
  // This type allows us to move keys when it is safe to do so. It is safe
  // for maps in which value_type and mutable_value_type are layout compatible.
  using slot_type = absl::container_internal::slot_type<Key, mapped_type>;
  using value_type = typename slot_type::value_type;
  using mutable_value_type = typename slot_type::mutable_value_type;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using reference = value_type &;
  using const_reference = const value_type &;

  using key_compare = typename map_params::common_params::key_compare;
  // Inherit from key_compare for empty base class optimization.
  struct value_compare : private key_compare {
    value_compare() = default;
    explicit value_compare(const key_compare &cmp) : key_compare(cmp) {}

    template <typename T, typename U>
    absl::conditional_t<btree_is_key_compare_to<key_compare>::value, int, bool>
    operator()(const T &left, const U &right) const {
      return key_compare::operator()(left.first, right.first);
    }
  };

  static const Key& key(const value_type &x) { return x.first; }
  static const Key& key(const mutable_value_type &x) { return x.first; }
};

// A parameters structure for holding the type parameters for a btree_set.
// Compare and Alloc should be nothrow copy-constructible.
template <typename Key, typename Compare, typename Alloc, int TargetNodeSize,
          bool Multi>
struct set_params
    : common_params<Key, Compare, Alloc, TargetNodeSize, sizeof(Key), Multi> {
  using mapped_type = void;
  using value_type = Key;
  using mutable_value_type = Key;
  // This type implements the necessary functions from the subtle::slot_type
  // interface.
  struct slot_type {
    value_type value;

    template <class... Args>
    static void construct(Alloc *alloc, slot_type *slot, Args &&... args) {
      absl::allocator_traits<Alloc>::construct(*alloc, &slot->value,
                                               std::forward<Args>(args)...);
    }

    static void construct(Alloc *alloc, slot_type *slot, slot_type *other) {
      absl::allocator_traits<Alloc>::construct(*alloc, &slot->value,
                                               std::move(other->value));
    }

    static void destroy(Alloc *alloc, slot_type *slot) {
      absl::allocator_traits<Alloc>::destroy(*alloc, &slot->value);
    }

    static void swap(Alloc * /*alloc*/, slot_type *a, slot_type *b) {
      using std::swap;
      swap(a->value, b->value);
    }

    static void move(Alloc * /*alloc*/, slot_type *src, slot_type *dest) {
      dest->value = std::move(src->value);
    }

    static void move(Alloc *alloc, slot_type *first, slot_type *last,
                     slot_type *result) {
      for (slot_type *src = first, *dest = result; src != last; ++src, ++dest)
        move(alloc, src, dest);
    }
  };
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using reference = value_type &;
  using const_reference = const value_type &;
  using value_compare = typename set_params::common_params::key_compare;

  static const Key& key(const value_type &x) { return x; }
};

// An adapter class that converts a lower-bound compare into an upper-bound
// compare. Note: there is no need to make a version of this adapter specialized
// for key-compare-to functors because the upper-bound (the first value greater
// than the input) is never an exact match.
template <typename Compare>
struct upper_bound_adapter {
  explicit upper_bound_adapter(const Compare &c) : comp(c) {}
  template <typename K, typename LK>
  bool operator()(const K &a, const LK &b) const {
    // Returns true when a is not greater than b.
    return !bool_compare_keys(comp, b, a);
  }

 private:
  Compare comp;
};

// A node in the btree holding. The same node type is used for both internal
// and leaf nodes in the btree, though the nodes are allocated in such a way
// that the children array is only valid in internal nodes.
template <typename Params>
class btree_node {
  using is_key_compare_to = typename Params::is_key_compare_to;
  using is_multi_container = typename Params::is_multi_container;
  using field_type = typename Params::node_count_type;
  using allocator_type = typename Params::allocator_type;
  using slot_type = typename Params::slot_type;

 public:
  using params_type = Params;
  using key_type = typename Params::key_type;
  using value_type = typename Params::value_type;
  using mutable_value_type = typename Params::mutable_value_type;
  using pointer = typename Params::pointer;
  using const_pointer = typename Params::const_pointer;
  using reference = typename Params::reference;
  using const_reference = typename Params::const_reference;
  using key_compare = typename Params::key_compare;
  using size_type = typename Params::size_type;
  using difference_type = typename Params::difference_type;

  // Btree's choice of binary search or linear search is a customization
  // point that can be configured via the key_compare and key_type.
  // Btree decides whether to use linear node search as follows:
  //   - If the comparator expresses a preference, use that.
  //   - Otherwise, if the key expresses a preference, use that.
  //   - Otherwise, if the key is arithmetic and the comparator is std::less or
  //     std::greater, choose linear.
  //   - Otherwise, choose binary.
  // See documentation for has_linear_node_search_preference and
  // prefers_linear_node_search above.
  // Might be wise to also configure linear search based on node-size.
  using use_linear_search = absl::conditional_t<
      has_linear_node_search_preference<key_compare>::value
          ? prefers_linear_node_search<key_compare>::value
          : has_linear_node_search_preference<key_type>::value
                ? prefers_linear_node_search<key_type>::value
                : std::is_arithmetic<key_type>::value &&
                      (std::is_same<std::less<key_type>, key_compare>::value ||
                       std::is_same<std::greater<key_type>,
                                    key_compare>::value),
      std::true_type, std::false_type>;

  // This class is organized by gtl::Layout as if it had the following
  // structure:
  //   // A pointer to the node's parent.
  //   btree_node *parent;
  //
  //   // The position of the node in the node's parent.
  //   field_type position;
  //   // The count of the number of values in the node.
  //   field_type count;
  //   // The maximum number of values the node can hold.
  //   field_type max_count;
  //   // An enum indicating whether the node is a leaf, internal, or a root.
  //   NodeType type;
  //
  //   // The array of values. Only the first `count` of these values have been
  //   // constructed and are valid.
  //   slot_type values[kNodeValues];
  //
  //   // The array of child pointers. The keys in children[i] are all less
  //   // than key(i). The keys in children[i + 1] are all greater than key(i).
  //   // There are always count + 1 children. Invalid for leaf nodes.
  //   btree_node *children[kNodeValues + 1];
  //
  // This class is never constructed or deleted. Instead, pointers to the layout
  // above are allocated, cast to btree_node*, and de-allocated within the btree
  // implementation.
  btree_node() = delete;
  ~btree_node() = delete;
  btree_node(btree_node const &) = delete;
  btree_node &operator=(btree_node const &) = delete;

 private:
  // This is the type of node allocation, which determines which fields in the
  // node layout are valid.
  enum NodeType : int8 {
    kLeaf,
    kInternal,
  };

  using layout_type =
      Layout<btree_node *, field_type, NodeType, slot_type, btree_node *>;
  constexpr static size_type SizeWithNValues(size_type n) {
    return layout_type(/*parent*/ 1,
                       /*position, count, max_count*/ 3, /*type*/ 1,
                       /*values*/ n,
                       /*children*/ 0)
        .AllocSize();
  }
  // A lower bound for the overhead of fields other than values in a leaf node.
  constexpr static size_type MinimumOverhead() {
    return SizeWithNValues(1) - sizeof(value_type);
  }

  // Compute how many values we can fit onto a leaf node taking into account
  // padding.
  constexpr static size_type NodeTargetValues(const int begin, const int end) {
    return begin == end ? begin
                        : SizeWithNValues((begin + end) / 2 + 1) >
                                  params_type::kTargetNodeSize
                              ? NodeTargetValues(begin, (begin + end) / 2)
                              : NodeTargetValues((begin + end) / 2 + 1, end);
  }

  enum {
    kTargetNodeSize = params_type::kTargetNodeSize,
    kNodeTargetValues = NodeTargetValues(0, params_type::kTargetNodeSize),

    // We need a minimum of 3 values per internal node in order to perform
    // splitting (1 value for the two nodes involved in the split and 1 value
    // propagated to the parent as the delimiter for the split).
    kNodeValues = kNodeTargetValues >= 3 ? kNodeTargetValues : 3,

    kExactMatch = 1 << 30,
    kMatchMask = kExactMatch - 1,
  };

  // Leaves can have less than kNodeValues values.
  constexpr static layout_type LeafLayout(const int max_values = kNodeValues) {
    return layout_type(/*parent*/ 1,
                       /*position, count, max_count*/ 3, /*type*/ 1,
                       /*values*/ max_values,
                       /*children*/ 0);
  }
  constexpr static layout_type InternalLayout() {
    return layout_type(/*parent*/ 1,
                       /*position, count, max_count*/ 3, /*type*/ 1,
                       /*values*/ kNodeValues,
                       /*children*/ kNodeValues + 1);
  }
  constexpr static size_type LeafSize(const int max_values = kNodeValues) {
    return LeafLayout(max_values).AllocSize();
  }
  constexpr static size_type InternalSize() {
    return InternalLayout().AllocSize();
  }
  constexpr static size_type Alignment() {
    static_assert(LeafLayout(1).Alignment() == InternalLayout().Alignment(),
                  "Alignment of all nodes must be equal.");
    return InternalLayout().Alignment();
  }

  // N is the index of the type in the Layout definition.
  // ElementType<N> is the Nth type in the Layout definition.
  template <size_type N>
  inline typename layout_type::template ElementType<N> *GetField() {
    // We assert that we don't read from values that aren't there.
    assert(N < 4 || !leaf());
    return InternalLayout().template Pointer<N>(reinterpret_cast<char *>(this));
  }
  template <size_type N>
  inline const typename layout_type::template ElementType<N> *GetField() const {
    assert(N < 4 || !leaf());
    return InternalLayout().template Pointer<N>(
        reinterpret_cast<const char *>(this));
  }
  NodeType type() const { return *GetField<2>(); }
  // This method is only called by the node init methods.
  void set_type(NodeType type) { *GetField<2>() = type; }
  void set_parent(btree_node *p) { *GetField<0>() = p; }
  field_type &mutable_count() { return GetField<1>()[1]; }
  slot_type *slot(int i) { return &GetField<3>()[i]; }
  const slot_type *slot(int i) const { return &GetField<3>()[i]; }
  void set_position(field_type v) { GetField<1>()[0] = v; }
  void set_count(field_type v) { GetField<1>()[1] = v; }
  void set_max_count(field_type v) { GetField<1>()[2] = v; }

 public:
  // Whether this is a leaf node or not. This value doesn't change after the
  // node is created.
  bool leaf() const { return *GetField<2>() == NodeType::kLeaf; }

  // Getter for the position of this node in its parent.
  field_type position() const { return GetField<1>()[0]; }

  // Getters for the number of values stored in this node.
  field_type count() const { return GetField<1>()[1]; }
  field_type max_count() const { return GetField<1>()[2]; }

  // Getter for the parent of this node.
  btree_node *parent() const { return *GetField<0>(); }
  // Getter for whether the node is the root of the tree. The parent of the
  // root of the tree is the leftmost node in the tree which is guaranteed to
  // be a leaf.
  bool is_root() const { return parent()->leaf(); }
  void make_root() {
    assert(parent()->is_root());
    set_parent(parent()->parent());
  }

  // Getters for the key/value at position i in the node.
  const key_type &key(int i) const { return params_type::key(slot(i)->value); }
  reference value(int i) { return slot(i)->value; }
  const_reference value(int i) const { return slot(i)->value; }

  // Getters/setter for the child at position i in the node.
  btree_node *child(int i) const { return GetField<4>()[i]; }
  btree_node *&mutable_child(int i) { return GetField<4>()[i]; }
  void clear_child(int i) { subtle::SanitizerPoisonObject(&mutable_child(i)); }
  void set_child(int i, btree_node *c) {
    subtle::SanitizerUnpoisonObject(&mutable_child(i));
    mutable_child(i) = c;
    c->set_position(i);
  }
  void init_child(int i, btree_node *c) {
    set_child(i, c);
    c->set_parent(this);
  }

  // Returns the position of the first value whose key is not less than k.
  template <typename K>
  int lower_bound(const K &k, const key_compare &comp) const {
    return use_linear_search::value ? linear_search(k, comp)
                                    : binary_search(k, comp);
  }
  // Returns the position of the first value whose key is greater than k.
  template <typename K>
  int upper_bound(const K &k, const key_compare &comp) const {
    auto upper_compare = upper_bound_adapter<key_compare>(comp);
    return use_linear_search::value ? linear_search(k, upper_compare)
                                    : binary_search(k, upper_compare);
  }

  template <typename K, typename Compare>
  int linear_search(const K &k, const Compare &comp) const {
    return btree_is_key_compare_to<Compare>::value
               ? linear_search_compare_to(k, 0, count(), comp)
               : linear_search_plain_compare(k, 0, count(), comp);
  }

  template <typename K, typename Compare>
  int binary_search(const K &k, const Compare &comp) const {
    return btree_is_key_compare_to<Compare>::value
               ? binary_search_compare_to(k, 0, count(), comp)
               : binary_search_plain_compare(k, 0, count(), comp);
  }

  // Returns the position of the first value whose key is not less than k using
  // linear search performed using plain compare.
  template <typename K, typename Compare>
  int linear_search_plain_compare(const K &k, int s, const int e,
                                  const Compare &comp) const {
    while (s < e) {
      if (!comp(key(s), k)) {
        break;
      }
      ++s;
    }
    return s;
  }

  // Returns the position of the first value whose key is not less than k using
  // linear search performed using compare-to.
  template <typename K, typename Compare>
  int linear_search_compare_to(const K &k, int s, const int e,
                               const Compare &comp) const {
    while (s < e) {
      const int c = comp(key(s), k);
      if (c == 0) {
        return s | kExactMatch;
      } else if (c > 0) {
        break;
      }
      ++s;
    }
    return s;
  }

  // Returns the position of the first value whose key is not less than k using
  // binary search performed using plain compare.
  template <typename K, typename Compare>
  int binary_search_plain_compare(const K &k, int s, int e,
                                  const Compare &comp) const {
    while (s != e) {
      const int mid = (s + e) >> 1;
      if (comp(key(mid), k)) {
        s = mid + 1;
      } else {
        e = mid;
      }
    }
    return s;
  }

  // Returns the position of the first value whose key is not less than k using
  // binary search performed using compare-to.
  template <typename K, typename CompareTo>
  int binary_search_compare_to(
      const K &k, int s, int e, const CompareTo &comp) const {
    if (is_multi_container::value) {
      int exact_match = 0;
      while (s != e) {
        const int mid = (s + e) >> 1;
        const int c = comp(key(mid), k);
        if (c < 0) {
          s = mid + 1;
        } else {
          e = mid;
          if (c == 0) {
            // Need to return the first value whose key is not less than k,
            // which requires continuing the binary search if this is a
            // multi-container.
            exact_match = kExactMatch;
          }
        }
      }
      return s | exact_match;
    } else {  // Not a multi-container.
      while (s != e) {
        const int mid = (s + e) >> 1;
        const int c = comp(key(mid), k);
        if (c < 0) {
          s = mid + 1;
        } else if (c > 0) {
          e = mid;
        } else {
          return mid | kExactMatch;
        }
      }
      return s;
    }
  }

  // Emplaces a value at position i, shifting all existing values and
  // children at positions >= i to the right by 1.
  template <typename... Args>
  void emplace_value(size_type i, allocator_type *alloc, Args &&... args);

  // Removes the value at position i, shifting all existing values and children
  // at positions > i to the left by 1.
  void remove_value(int i, allocator_type *alloc);

  // Rebalances a node with its right sibling.
  void rebalance_right_to_left(int to_move, btree_node *right,
                               allocator_type *alloc);
  void rebalance_left_to_right(int to_move, btree_node *right,
                               allocator_type *alloc);

  // Splits a node, moving a portion of the node's values to its right sibling.
  void split(int insert_position, btree_node *dest, allocator_type *alloc);

  // Merges a node with its right sibling, moving all of the values and the
  // delimiting key in the parent node onto itself.
  void merge(btree_node *sibling, allocator_type *alloc);

  // Swap the contents of "this" and "src".
  void swap(btree_node *src, allocator_type *alloc);

  // Node allocation/deletion routines.
  static btree_node *init_leaf(btree_node *n, btree_node *parent,
                               int max_count) {
    n->set_parent(parent);
    n->set_position(0);
    n->set_max_count(max_count);
    n->set_count(0);
    n->set_type(NodeType::kLeaf);
    subtle::SanitizerPoisonMemoryRegion(n->slot(0),
                                        max_count * sizeof(slot_type));
    return n;
  }
  static btree_node *init_internal(btree_node *n, btree_node *parent) {
    init_leaf(n, parent, kNodeValues);
    n->set_type(NodeType::kInternal);
    subtle::SanitizerPoisonMemoryRegion(
        &n->mutable_child(0), (kNodeValues + 1) * sizeof(btree_node *));
    return n;
  }
  void destroy(allocator_type *alloc) {
    for (int i = 0; i < count(); ++i) {
      value_destroy(i, alloc);
    }
  }

 public:
  // Exposed only for tests.
  static bool testonly_uses_linear_node_search() {
    return use_linear_search::value;
  }

 private:
  template <typename... Args>
  void value_init(const size_type i, allocator_type *alloc, Args &&... args) {
    subtle::SanitizerUnpoisonObject(slot(i));
    slot_type::construct(alloc, slot(i), std::forward<Args>(args)...);
  }
  void value_destroy(const size_type i, allocator_type *alloc) {
    slot_type::destroy(alloc, slot(i));
    subtle::SanitizerPoisonObject(slot(i));
  }

  // Move n values starting at value i in this node into the values starting at
  // value j in node x.
  void uninitialized_move_n(const size_type n, const size_type i,
                            const size_type j, btree_node *x,
                            allocator_type *alloc) {
    subtle::SanitizerUnpoisonMemoryRegion(x->slot(j), n * sizeof(slot_type));
    for (slot_type *src = slot(i), *end = src + n, *dest = x->slot(j);
         src != end; ++src, ++dest) {
      slot_type::construct(alloc, dest, src);
    }
  }

  // Destroys a range of n values, starting at index i.
  void value_destroy_n(const size_type i, const size_type n,
                       allocator_type *alloc) {
    for (int j = 0; j < n; ++j) {
      value_destroy(i + j, alloc);
    }
  }

  template <typename P>
  friend class btree;
};

template <typename Node, typename Reference, typename Pointer>
struct btree_iterator {
 private:
  using key_type = typename Node::key_type;
  using size_type = typename Node::size_type;
  using params_type = typename Node::params_type;

  using node_type = Node;
  using normal_node = typename std::remove_const<Node>::type;
  using const_node = const Node;
  using normal_pointer = typename params_type::pointer;
  using normal_reference = typename params_type::reference;
  using const_pointer = typename params_type::const_pointer;
  using const_reference = typename params_type::const_reference;

  using iterator =
      btree_iterator<normal_node, normal_reference, normal_pointer>;
  using const_iterator =
      btree_iterator<const_node, const_reference, const_pointer>;

 public:
  // These aliases are public for std::iterator_traits.
  using difference_type = typename Node::difference_type;
  using value_type = typename params_type::value_type;
  using pointer = Pointer;
  using reference = Reference;
  using iterator_category = std::bidirectional_iterator_tag;

  btree_iterator() : node(nullptr), position(-1) {}
  btree_iterator(Node *n, int p) : node(n), position(p) {}

  // NOTE: this SFINAE allows for implicit conversions from iterator to
  // const_iterator, but it specifically avoids defining copy constructors so
  // that btree_iterator can be trivially copyable. This is for performance and
  // binary size reasons.
  template <
      typename N, typename R, typename P,
      absl::enable_if_t<
          std::is_same<btree_iterator<N, R, P>, iterator>::value &&
              !std::is_same<btree_iterator<N, R, P>, btree_iterator>::value,
          int> = 0>
  btree_iterator(const btree_iterator<N, R, P> &x)  // NOLINT
      : node(x.node), position(x.position) {}

 private:
  // Increment/decrement the iterator.
  void increment() {
    if (node->leaf() && ++position < node->count()) {
      return;
    }
    increment_slow();
  }
  void increment_slow();

  void decrement() {
    if (node->leaf() && --position >= 0) {
      return;
    }
    decrement_slow();
  }
  void decrement_slow();

 public:
  bool operator==(const const_iterator &x) const {
    return node == x.node && position == x.position;
  }
  bool operator!=(const const_iterator &x) const {
    return node != x.node || position != x.position;
  }

  // Accessors for the key/value the iterator is pointing at.
  reference operator*() const {
    return node->value(position);
  }
  pointer operator->() const {
    return &node->value(position);
  }

  btree_iterator& operator++() {
    increment();
    return *this;
  }
  btree_iterator& operator--() {
    decrement();
    return *this;
  }
  btree_iterator operator++(int) {
    btree_iterator tmp = *this;
    ++*this;
    return tmp;
  }
  btree_iterator operator--(int) {
    btree_iterator tmp = *this;
    --*this;
    return tmp;
  }

 private:
  template <typename Params>
  friend class btree;
  template <typename N, typename R, typename P>
  friend struct btree_iterator;
  template <typename TreeType, typename CheckerType>
  friend class base_checker;

  const key_type &key() const { return node->key(position); }

  // The node in the tree the iterator is pointing at.
  Node *node;
  // The position within the node of the tree the iterator is pointing at.
  int position;
};

// Approximation of std::is_trivially_copyable (which is currently unsupported).
template <typename T>
using is_trivially_copyable = absl::conjunction<
    absl::is_trivially_copy_constructible<T>,
    absl::disjunction<absl::is_trivially_copy_assignable<T>,
                      absl::negation<std::is_copy_assignable<T>>>,
    absl::is_trivially_destructible<T>>;

template <typename Params>
class btree {
  using node_type = btree_node<Params>;
  using is_key_compare_to = typename Params::is_key_compare_to;

  template <typename K>
  using const_lookup_key_reference = absl::conditional_t<
      is_comparator_transparent<typename Params::key_compare>::value, const K &,
      const typename Params::key_type &>;

  enum {
    kNodeValues = node_type::kNodeValues,
    kMinNodeValues = kNodeValues / 2,
    kExactMatch = node_type::kExactMatch,
    kMatchMask = node_type::kMatchMask,
  };

  struct node_stats {
    using size_type = typename Params::size_type;

    node_stats(size_type l, size_type i)
        : leaf_nodes(l),
          internal_nodes(i) {
    }

    node_stats& operator+=(const node_stats &x) {
      leaf_nodes += x.leaf_nodes;
      internal_nodes += x.internal_nodes;
      return *this;
    }

    size_type leaf_nodes;
    size_type internal_nodes;
  };

 public:
  using key_type = typename Params::key_type;
  using mapped_type = typename Params::mapped_type;
  using value_type = typename Params::value_type;
  using size_type = typename Params::size_type;
  using difference_type = typename Params::difference_type;
  using key_compare = typename Params::key_compare;
  using value_compare = typename Params::value_compare;
  using allocator_type = typename Params::allocator_type;
  using reference = typename Params::reference;
  using const_reference = typename Params::const_reference;
  using pointer = typename Params::pointer;
  using const_pointer = typename Params::const_pointer;
  using iterator = btree_iterator<node_type, reference, pointer>;
  using const_iterator = typename iterator::const_iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Internal types made public for use by btree_container types.
  using params_type = Params;
  using mutable_value_type = typename Params::mutable_value_type;
  using slot_type = typename Params::slot_type;

 private:
  // Copies the values in x into this btree in their order in x.
  // This btree must be empty before this method is called.
  // This method is used in copy construction and copy assignment.
  void copy_values_in_order(const btree &x);

 public:
  btree(const key_compare &comp, const allocator_type &alloc);

  btree(const btree &x);
  btree(btree &&x) noexcept
      : root_(std::move(x.root_)),
        rightmost_(absl::exchange(x.rightmost_, nullptr)),
        size_(absl::exchange(x.size_, 0)) {
    x.mutable_root() = nullptr;
  }

  ~btree() {
    static_assert(std::is_nothrow_copy_constructible<key_compare>::value,
                  "Key comparison must be nothrow copy constructible");
    static_assert(std::is_nothrow_copy_constructible<allocator_type>::value,
                  "Allocator must be nothrow copy constructible");
    static_assert(is_trivially_copyable<iterator>::value,
                  "iterator not trivially copyable.");
    clear();
  }

  // Assign the contents of x to *this.
  btree &operator=(const btree &x);

  btree &operator=(btree &&x) noexcept {
    clear();
    swap(x);
    return *this;
  }

  iterator begin() {
    return iterator(leftmost(), 0);
  }
  const_iterator begin() const {
    return const_iterator(leftmost(), 0);
  }
  iterator end() {
    return iterator(rightmost_,
                    rightmost_ != nullptr ? rightmost_->count() : 0);
  }
  const_iterator end() const {
    return const_iterator(rightmost_,
                          rightmost_ != nullptr ? rightmost_->count() : 0);
  }
  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  reverse_iterator rend() {
    return reverse_iterator(begin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // Finds the first element whose key is not less than key.
  template <typename K>
  iterator lower_bound(const K &key) {
    return internal_end(
        internal_lower_bound(key, iterator(root(), 0)));
  }
  template <typename K>
  const_iterator lower_bound(const K &key) const {
    return internal_end(
        internal_lower_bound(key, const_iterator(root(), 0)));
  }

  // Finds the first element whose key is greater than key.
  template <typename K>
  iterator upper_bound(const K &key) {
    return internal_end(
        internal_upper_bound(key, iterator(root(), 0)));
  }
  template <typename K>
  const_iterator upper_bound(const K &key) const {
    return internal_end(
        internal_upper_bound(key, const_iterator(root(), 0)));
  }

  // Finds the range of values which compare equal to key. The first member of
  // the returned pair is equal to lower_bound(key). The second member pair of
  // the pair is equal to upper_bound(key).
  template <typename K>
  std::pair<iterator, iterator> equal_range(const K &key) {
    const_lookup_key_reference<K> lookup_key(key);
    return std::make_pair(lower_bound(lookup_key), upper_bound(lookup_key));
  }
  template <typename K>
  std::pair<const_iterator, const_iterator> equal_range(const K &key) const {
    const_lookup_key_reference<K> lookup_key(key);
    return std::make_pair(lower_bound(lookup_key), upper_bound(lookup_key));
  }

  // Inserts a value into the btree only if it does not already exist. The
  // boolean return value indicates whether insertion succeeded or failed.
  template <typename... Args>
  std::pair<iterator, bool> insert_unique(const key_type &key, Args &&... args);

  // Insert with hint. Check to see if the value should be placed immediately
  // before position in the tree. If it does, then the insertion will take
  // amortized constant time. If not, the insertion will take amortized
  // logarithmic time as if a call to insert_unique(v) were made.
  template <typename... Args>
  iterator insert_hint_unique(iterator position, const key_type &key,
                              Args &&... args);

  // Insert a range of values into the btree.
  template <typename InputIterator>
  void insert_iterator_unique(InputIterator b, InputIterator e);

  // Inserts a value into the btree.
  template <typename ValueType>
  iterator insert_multi(const key_type &key, ValueType &&v);

  // Inserts a value into the btree.
  template <typename ValueType>
  iterator insert_multi(ValueType &&v) {
    return insert_multi(params_type::key(v), std::forward<ValueType>(v));
  }

  // Insert with hint. Check to see if the value should be placed immediately
  // before position in the tree. If it does, then the insertion will take
  // amortized constant time. If not, the insertion will take amortized
  // logarithmic time as if a call to insert_multi(v) were made.
  template <typename ValueType>
  iterator insert_hint_multi(iterator position, ValueType &&v);

  // Insert a range of values into the btree.
  template <typename InputIterator>
  void insert_iterator_multi(InputIterator b, InputIterator e);

  // Erase the specified iterator from the btree. The iterator must be valid
  // (i.e. not equal to end()).  Return an iterator pointing to the node after
  // the one that was erased (or end() if none exists).
  iterator erase(iterator iter);

  // Erases range. Returns the number of keys erased.
  int erase(iterator begin, iterator end);

  // Erases the specified key from the btree. Returns 1 if an element was
  // erased and 0 otherwise.
  template <typename K>
  int erase_unique(const K &key);

  // Erases all of the entries matching the specified key from the
  // btree. Returns the number of elements erased.
  template <typename K>
  int erase_multi(const K &key);

  // Finds the iterator corresponding to a key or returns end() if the key is
  // not present.
  template <typename K>
  iterator find_unique(const K &key) {
    return internal_end(
        internal_find_unique(key, iterator(root(), 0)));
  }
  template <typename K>
  const_iterator find_unique(const K &key) const {
    return internal_end(
        internal_find_unique(key, const_iterator(root(), 0)));
  }
  template <typename K>
  iterator find_multi(const K &key) {
    return internal_end(
        internal_find_multi(key, iterator(root(), 0)));
  }
  template <typename K>
  const_iterator find_multi(const K &key) const {
    return internal_end(
        internal_find_multi(key, const_iterator(root(), 0)));
  }

  // Returns a count of the number of times the key appears in the btree.
  template <typename K>
  size_type count_unique(const K &key) const {
    const_iterator begin = internal_find_unique(
        key, const_iterator(root(), 0));
    if (!begin.node) {
      // The key doesn't exist in the tree.
      return 0;
    }
    return 1;
  }
  // Returns a count of the number of times the key appears in the btree.
  template <typename K>
  size_type count_multi(const K &key) const {
    const auto range = equal_range(key);
    return std::distance(range.first, range.second);
  }

  // Clear the btree, deleting all of the values it contains.
  void clear();

  // Swap the contents of *this and x.
  void swap(btree &x);

  const key_compare &key_comp() const noexcept {
    return root_.template get<0>();
  }
  template <typename K, typename LK>
  bool compare_keys(const K &x, const LK &y) const {
    return bool_compare_keys(key_comp(), x, y);
  }

  value_compare value_comp() const { return value_compare(key_comp()); }

  // Verifies the structure of the btree.
  void verify() const;

  // Size routines.
  size_type size() const { return size_; }
  size_type max_size() const { return std::numeric_limits<size_type>::max(); }
  bool empty() const { return size_ == 0; }

  // The height of the btree. An empty tree will have height 0.
  size_type height() const {
    size_type h = 0;
    if (root()) {
      // Count the length of the chain from the leftmost node up to the
      // root. We actually count from the root back around to the level below
      // the root, but the calculation is the same because of the circularity
      // of that traversal.
      const node_type *n = root();
      do {
        ++h;
        n = n->parent();
      } while (n != root());
    }
    return h;
  }

  // The number of internal, leaf and total nodes used by the btree.
  size_type leaf_nodes() const {
    return internal_stats(root()).leaf_nodes;
  }
  size_type internal_nodes() const {
    return internal_stats(root()).internal_nodes;
  }
  size_type nodes() const {
    node_stats stats = internal_stats(root());
    return stats.leaf_nodes + stats.internal_nodes;
  }

  // The total number of bytes used by the btree.
  size_type bytes_used() const {
    node_stats stats = internal_stats(root());
    if (stats.leaf_nodes == 1 && stats.internal_nodes == 0) {
      return sizeof(*this) +
             node_type::LeafSize(root()->max_count());
    } else {
      return sizeof(*this) +
             stats.leaf_nodes * node_type::LeafSize() +
             stats.internal_nodes * node_type::InternalSize();
    }
  }

  // The average number of bytes used per value stored in the btree.
  static double average_bytes_per_value() {
    // Returns the number of bytes per value on a leaf node that is 75%
    // full. Experimentally, this matches up nicely with the computed number of
    // bytes per value in trees that had their values inserted in random order.
    return node_type::LeafSize() / (kNodeValues * 0.75);
  }

  // The fullness of the btree. Computed as the number of elements in the btree
  // divided by the maximum number of elements a tree with the current number
  // of nodes could hold. A value of 1 indicates perfect space
  // utilization. Smaller values indicate space wastage.
  double fullness() const {
    return static_cast<double>(size()) / (nodes() * kNodeValues);
  }
  // The overhead of the btree structure in bytes per node. Computed as the
  // total number of bytes used by the btree minus the number of bytes used for
  // storing elements divided by the number of elements.
  double overhead() const {
    if (empty()) {
      return 0.0;
    }
    return (bytes_used() - size() * sizeof(value_type)) /
           static_cast<double>(size());
  }

  // The allocator used by the btree.
  allocator_type get_allocator() const {
    return allocator();
  }

 private:
  // Internal accessor routines.
  node_type *root() { return root_.template get<2>(); }
  const node_type *root() const { return root_.template get<2>(); }
  node_type *&mutable_root() noexcept { return root_.template get<2>(); }
  key_compare *mutable_key_comp() noexcept { return &root_.template get<0>(); }

  // The leftmost node is stored as the parent of the root node.
  node_type *leftmost() { return root() ? root()->parent() : nullptr; }
  const node_type *leftmost() const {
    return root() ? root()->parent() : nullptr;
  }

  // Allocator routines.
  allocator_type *mutable_allocator() noexcept {
    return &root_.template get<1>();
  }
  const allocator_type &allocator() const noexcept {
    return root_.template get<1>();
  }

  // Allocates a correctly aligned node of at least size bytes using the
  // allocator.
  node_type *allocate(const size_type size) {
    return reinterpret_cast<node_type *>(
        subtle::Allocate<node_type::Alignment()>(mutable_allocator(), size));
  }

  // Node creation/deletion routines.
  node_type* new_internal_node(node_type *parent) {
    node_type *p = allocate(node_type::InternalSize());
    return node_type::init_internal(p, parent);
  }
  node_type* new_leaf_node(node_type *parent) {
    node_type *p = allocate(node_type::LeafSize());
    return node_type::init_leaf(p, parent, kNodeValues);
  }
  node_type *new_leaf_root_node(const int max_count) {
    node_type *p = allocate(node_type::LeafSize(max_count));
    return node_type::init_leaf(p, p, max_count);
  }

  // Deallocates a node of a certain size in bytes using the allocator.
  void deallocate(const size_type size, node_type *node) {
    subtle::Deallocate<node_type::Alignment()>(mutable_allocator(), node, size);
  }

  void delete_internal_node(node_type *node) {
    node->destroy(mutable_allocator());
    deallocate(node_type::InternalSize(), node);
  }
  void delete_leaf_node(node_type *node) {
    node->destroy(mutable_allocator());
    deallocate(node_type::LeafSize(node->max_count()), node);
  }

  // Rebalances or splits the node iter points to.
  void rebalance_or_split(iterator *iter);

  // Merges the values of left, right and the delimiting key on their parent
  // onto left, removing the delimiting key and deleting right.
  void merge_nodes(node_type *left, node_type *right);

  // Tries to merge node with its left or right sibling, and failing that,
  // rebalance with its left or right sibling. Returns true if a merge
  // occurred, at which point it is no longer valid to access node. Returns
  // false if no merging took place.
  bool try_merge_or_rebalance(iterator *iter);

  // Tries to shrink the height of the tree by 1.
  void try_shrink();

  iterator internal_end(iterator iter) {
    return iter.node ? iter : end();
  }
  const_iterator internal_end(const_iterator iter) const {
    return iter.node ? iter : end();
  }

  // Emplaces a value into the btree immediately before iter. Requires that
  // key(v) <= iter.key() and (--iter).key() <= key(v).
  template <typename... Args>
  iterator internal_emplace(iterator iter, Args &&... args);

  // Returns an iterator pointing to the first value >= the value "iter" is
  // pointing at. Note that "iter" might be pointing to an invalid location as
  // iter.position == iter.node->count(). This routine simply moves iter up in
  // the tree to a valid location.
  template <typename IterType>
  static IterType internal_last(IterType iter);

  // Returns an iterator pointing to the leaf position at which key would
  // reside in the tree. We provide 2 versions of internal_locate. The first
  // version (internal_locate_plain_compare) always returns 0 for the second
  // field of the pair. The second version (internal_locate_compare_to) is for
  // the key-compare-to specialization and returns either kExactMatch (if the
  // key was found in the tree) or -kExactMatch (if it wasn't) in the second
  // field of the pair. The compare_to specialization allows the caller to
  // avoid a subsequent comparison to determine if an exact match was made,
  // speeding up string, cord and string_view keys.
  template <typename K, typename IterType>
  std::pair<IterType, int> internal_locate(
      const K &key, IterType iter) const;
  template <typename K, typename IterType>
  std::pair<IterType, int> internal_locate_plain_compare(
      const K &key, IterType iter) const;
  template <typename K, typename IterType>
  std::pair<IterType, int> internal_locate_compare_to(
      const K &key, IterType iter) const;

  // Internal routine which implements lower_bound().
  template <typename K, typename IterType>
  IterType internal_lower_bound(
      const K &key, IterType iter) const;

  // Internal routine which implements upper_bound().
  template <typename K, typename IterType>
  IterType internal_upper_bound(
      const K &key, IterType iter) const;

  // Internal routine which implements find_unique().
  template <typename K, typename IterType>
  IterType internal_find_unique(
      const K &key, IterType iter) const;

  // Internal routine which implements find_multi().
  template <typename K, typename IterType>
  IterType internal_find_multi(
      const K &key, IterType iter) const;

  // Deletes a node and all of its children.
  void internal_clear(node_type *node);

  // Verifies the tree structure of node.
  int internal_verify(const node_type *node,
                      const key_type *lo, const key_type *hi) const;

  node_stats internal_stats(const node_type *node) const {
    if (!node) {
      return node_stats(0, 0);
    }
    if (node->leaf()) {
      return node_stats(1, 0);
    }
    node_stats res(0, 1);
    for (int i = 0; i <= node->count(); ++i) {
      res += internal_stats(node->child(i));
    }
    return res;
  }

 public:
  // Exposed only for tests.
  static bool testonly_uses_linear_node_search() {
    return node_type::testonly_uses_linear_node_search();
  }

 private:
  // We use compressed tuple in order to save space because key_compare and
  // allocator_type are usually empty.
  absl::container_internal::CompressedTuple<key_compare, allocator_type,
                                            node_type *>
      root_;

  // A pointer to the rightmost node. Note that the leftmost node is stored as
  // the root's parent.
  node_type *rightmost_;

  // Number of values.
  size_type size_;

  // Verify that key_compare returns an int or bool, as appropriate
  // depending on the value of is_key_compare_to.
  static_assert(std::is_same<absl::result_of_t<key_compare(key_type, key_type)>,
                             absl::conditional_t<is_key_compare_to::value, int,
                                                 bool>>::value,
                "key comparison function must return bool");

  // Note: We assert that kTargetValues, which is computed from
  // Params::kTargetNodeSize, must fit the node_type::field_type.
  static_assert(kNodeValues <
                    (1 << (8 * sizeof(typename node_type::field_type))),
                "target node size too large");

  // Test the assumption made in setting kNodeValueSpace.
  static_assert(node_type::MinimumOverhead() >= sizeof(void *) + 4,
                "node space assumption incorrect");
};

////
// btree_node methods
template <typename P>
template <typename... Args>
inline void btree_node<P>::emplace_value(const size_type i,
                                         allocator_type *alloc,
                                         Args &&... args) {
  assert(i <= count());
  // Shift old values to create space for new value and then construct it in
  // place.
  if (i < count()) {
    value_init(count(), alloc, slot(count() - 1));
    for (size_type j = count() - 1; j > i; --j)
      slot_type::move(alloc, slot(j - 1), slot(j));
    value_destroy(i, alloc);
  }
  value_init(i, alloc, std::forward<Args>(args)...);
  set_count(count() + 1);

  if (!leaf() && count() > i + 1) {
    for (int j = count(); j > i + 1; --j) {
      set_child(j, child(j - 1));
    }
    clear_child(i + 1);
  }
}

template <typename P>
inline void btree_node<P>::remove_value(const int i, allocator_type *alloc) {
  if (!leaf() && count() > i + 1) {
    assert(child(i + 1)->count() == 0);
    for (size_type j = i + 1; j < count(); ++j) {
      set_child(j, child(j + 1));
    }
    clear_child(count());
  }

  slot_type::move(alloc, slot(i + 1), slot(count()), slot(i));
  value_destroy(count() - 1, alloc);
  set_count(count() - 1);
}

template <typename P>
void btree_node<P>::rebalance_right_to_left(const int to_move,
                                            btree_node *right,
                                            allocator_type *alloc) {
  assert(parent() == right->parent());
  assert(position() + 1 == right->position());
  assert(right->count() >= count());
  assert(to_move >= 1);
  assert(to_move <= right->count());

  // 1) Move the delimiting value in the parent to the left node.
  value_init(count(), alloc, parent()->slot(position()));

  // 2) Move the (to_move - 1) values from the right node to the left node.
  right->uninitialized_move_n(to_move - 1, 0, count() + 1, this, alloc);

  // 3) Move the new delimiting value to the parent from the right node.
  slot_type::move(alloc, right->slot(to_move - 1), parent()->slot(position()));

  // 4) Shift the values in the right node to their correct position.
  slot_type::move(alloc, right->slot(to_move), right->slot(right->count()),
                  right->slot(0));

  // 5) Destroy the now-empty to_move entries in the right node.
  right->value_destroy_n(right->count() - to_move, to_move, alloc);

  if (!leaf()) {
    // Move the child pointers from the right to the left node.
    for (int i = 0; i < to_move; ++i) {
      init_child(count() + i + 1, right->child(i));
    }
    for (int i = 0; i <= right->count() - to_move; ++i) {
      assert(i + to_move <= right->max_count());
      right->init_child(i, right->child(i + to_move));
      right->clear_child(i + to_move);
    }
  }

  // Fixup the counts on the left and right nodes.
  set_count(count() + to_move);
  right->set_count(right->count() - to_move);
}

template <typename P>
void btree_node<P>::rebalance_left_to_right(const int to_move,
                                            btree_node *right,
                                            allocator_type *alloc) {
  assert(parent() == right->parent());
  assert(position() + 1 == right->position());
  assert(count() >= right->count());
  assert(to_move >= 1);
  assert(to_move <= count());

  // Values in the right node are shifted to the right to make room for the
  // new to_move values. Then, the delimiting value in the parent and the
  // other (to_move - 1) values in the left node are moved into the right node.
  // Lastly, a new delimiting value is moved from the left node into the
  // parent, and the remaining empty left node entries are destroyed.

  if (right->count() >= to_move) {
    // The original location of the right->count() values are sufficient to hold
    // the new to_move entries from the parent and left node.

    // 1) Shift existing values in the right node to their correct positions.
    right->uninitialized_move_n(to_move, right->count() - to_move,
                                right->count(), right, alloc);
    for (slot_type *src = right->slot(right->count() - to_move - 1),
                   *dest = right->slot(right->count() - 1),
                   *end = right->slot(0);
         src >= end; --src, --dest) {
      slot_type::move(alloc, src, dest);
    }

    // 2) Move the delimiting value in the parent to the right node.
    slot_type::move(alloc, parent()->slot(position()),
                    right->slot(to_move - 1));

    // 3) Move the (to_move - 1) values from the left node to the right node.
    slot_type::move(alloc, slot(count() - (to_move - 1)), slot(count()),
                    right->slot(0));
  } else {
    // The right node does not have enough initialized space to hold the new
    // to_move entries, so part of them will move to uninitialized space.

    // 1) Shift existing values in the right node to their correct positions.
    right->uninitialized_move_n(right->count(), 0, to_move, right, alloc);

    // 2) Move the delimiting value in the parent to the right node.
    right->value_init(to_move - 1, alloc, parent()->slot(position()));

    // 3) Move the (to_move - 1) values from the left node to the right node.
    const size_type uninitialized_remaining = to_move - right->count() - 1;
    uninitialized_move_n(uninitialized_remaining,
                         count() - uninitialized_remaining, right->count(),
                         right, alloc);
    slot_type::move(alloc, slot(count() - (to_move - 1)),
                    slot(count() - uninitialized_remaining), right->slot(0));
  }

  // 4) Move the new delimiting value to the parent from the left node.
  slot_type::move(alloc, slot(count() - to_move), parent()->slot(position()));

  // 5) Destroy the now-empty to_move entries in the left node.
  value_destroy_n(count() - to_move, to_move, alloc);

  if (!leaf()) {
    // Move the child pointers from the left to the right node.
    for (int i = right->count(); i >= 0; --i) {
      right->init_child(i + to_move, right->child(i));
      right->clear_child(i);
    }
    for (int i = 1; i <= to_move; ++i) {
      right->init_child(i - 1, child(count() - to_move + i));
      clear_child(count() - to_move + i);
    }
  }

  // Fixup the counts on the left and right nodes.
  set_count(count() - to_move);
  right->set_count(right->count() + to_move);
}

template <typename P>
void btree_node<P>::split(const int insert_position, btree_node *dest,
                          allocator_type *alloc) {
  assert(dest->count() == 0);

  // We bias the split based on the position being inserted. If we're
  // inserting at the beginning of the left node then bias the split to put
  // more values on the right node. If we're inserting at the end of the
  // right node then bias the split to put more values on the left node.
  if (insert_position == 0) {
    dest->set_count(count() - 1);
  } else if (insert_position == max_count()) {
    dest->set_count(0);
  } else {
    dest->set_count(count() / 2);
  }
  set_count(count() - dest->count());
  assert(count() >= 1);

  // Move values from the left sibling to the right sibling.
  uninitialized_move_n(dest->count(), count(), 0, dest, alloc);

  // Destroy the now-empty entries in the left node.
  value_destroy_n(count(), dest->count(), alloc);

  // The split key is the largest value in the left sibling.
  set_count(count() - 1);
  parent()->emplace_value(position(), alloc, slot(count()));
  value_destroy(count(), alloc);
  parent()->init_child(position() + 1, dest);

  if (!leaf()) {
    for (int i = 0; i <= dest->count(); ++i) {
      assert(child(count() + i + 1) != nullptr);
      dest->init_child(i, child(count() + i + 1));
      clear_child(count() + i + 1);
    }
  }
}

template <typename P>
void btree_node<P>::merge(btree_node *src, allocator_type *alloc) {
  assert(parent() == src->parent());
  assert(position() + 1 == src->position());

  // Move the delimiting value to the left node.
  value_init(count(), alloc, parent()->slot(position()));

  // Move the values from the right to the left node.
  src->uninitialized_move_n(src->count(), 0, count() + 1, this, alloc);

  // Destroy the now-empty entries in the right node.
  src->value_destroy_n(0, src->count(), alloc);

  if (!leaf()) {
    // Move the child pointers from the right to the left node.
    for (int i = 0; i <= src->count(); ++i) {
      init_child(count() + i + 1, src->child(i));
      src->clear_child(i);
    }
  }

  // Fixup the counts on the src and dest nodes.
  set_count(1 + count() + src->count());
  src->set_count(0);

  // Remove the value on the parent node.
  parent()->remove_value(position(), alloc);
}

template <typename P>
void btree_node<P>::swap(btree_node *x, allocator_type *alloc) {
  using std::swap;
  assert(leaf() == x->leaf());

  // Determine which is the smaller/larger node.
  btree_node *smaller = this, *larger = x;
  if (smaller->count() > larger->count()) {
    swap(smaller, larger);
  }

  // Swap the values.
  for (slot_type *a = smaller->slot(0), *b = larger->slot(0),
                 *end = a + smaller->count();
       a != end; ++a, ++b) {
    slot_type::swap(alloc, a, b);
  }

  // Move values that can't be swapped.
  const size_type to_move = larger->count() - smaller->count();
  larger->uninitialized_move_n(to_move, smaller->count(), smaller->count(),
                               smaller, alloc);
  larger->value_destroy_n(smaller->count(), to_move, alloc);

  if (!leaf()) {
    // Swap the child pointers.
    std::swap_ranges(&smaller->mutable_child(0),
                     &smaller->mutable_child(smaller->count() + 1),
                     &larger->mutable_child(0));
    // Update swapped children's parent pointers.
    int i = 0;
    for (; i <= smaller->count(); ++i) {
      smaller->child(i)->set_parent(smaller);
      larger->child(i)->set_parent(larger);
    }
    // Move the child pointers that couldn't be swapped.
    for (; i <= larger->count(); ++i) {
      smaller->init_child(i, larger->child(i));
      larger->clear_child(i);
    }
  }

  // Swap the counts.
  swap(mutable_count(), x->mutable_count());
}

////
// btree_iterator methods
template <typename N, typename R, typename P>
void btree_iterator<N, R, P>::increment_slow() {
  if (node->leaf()) {
    assert(position >= node->count());
    btree_iterator save(*this);
    while (position == node->count() && !node->is_root()) {
      assert(node->parent()->child(node->position()) == node);
      position = node->position();
      node = node->parent();
    }
    if (position == node->count()) {
      *this = save;
    }
  } else {
    assert(position < node->count());
    node = node->child(position + 1);
    while (!node->leaf()) {
      node = node->child(0);
    }
    position = 0;
  }
}

template <typename N, typename R, typename P>
void btree_iterator<N, R, P>::decrement_slow() {
  if (node->leaf()) {
    assert(position <= -1);
    btree_iterator save(*this);
    while (position < 0 && !node->is_root()) {
      assert(node->parent()->child(node->position()) == node);
      position = node->position() - 1;
      node = node->parent();
    }
    if (position < 0) {
      *this = save;
    }
  } else {
    assert(position >= 0);
    node = node->child(position);
    while (!node->leaf()) {
      node = node->child(node->count());
    }
    position = node->count() - 1;
  }
}

////
// btree methods
template <typename P>
void btree<P>::copy_values_in_order(const btree &x) {
  assert(empty());

  // We can avoid key comparisons because we know the order of the
  // values is the same order we'll store them in.
  const_iterator iter = x.begin();
  if (iter == x.end()) return;
  insert_multi(*iter);
  ++iter;
  for (; iter != x.end(); ++iter) {
    // If the btree is not empty, we can just insert the new value at the end
    // of the tree!
    internal_emplace(end(), *iter);
  }
}

template <typename P>
btree<P>::btree(const key_compare &comp, const allocator_type &alloc)
    : root_(comp, alloc, nullptr), rightmost_(nullptr), size_(0) {}

template <typename P>
btree<P>::btree(const btree &x) : btree(x.key_comp(), x.allocator()) {
  copy_values_in_order(x);
}

template <typename P>
template <typename... Args>
auto btree<P>::insert_unique(const key_type &key, Args &&... args)
    -> std::pair<iterator, bool> {
  if (empty()) {
    mutable_root() = rightmost_ = new_leaf_root_node(1);
  }

  std::pair<iterator, int> res = internal_locate(key, iterator(root(), 0));
  iterator &iter = res.first;
  if (res.second == kExactMatch) {
    // The key already exists in the tree, do nothing.
    return std::make_pair(internal_last(iter), false);
  } else if (!res.second) {
    iterator last = internal_last(iter);
    if (last.node && !compare_keys(key, last.key())) {
      // The key already exists in the tree, do nothing.
      return std::make_pair(last, false);
    }
  }

  return std::make_pair(internal_emplace(iter, std::forward<Args>(args)...),
                        true);
}

template <typename P>
template <typename... Args>
inline auto btree<P>::insert_hint_unique(iterator position, const key_type &key,
                                         Args &&... args) -> iterator {
  if (!empty()) {
    if (position == end() || compare_keys(key, position.key())) {
      iterator prev = position;
      if (position == begin() || compare_keys((--prev).key(), key)) {
        // prev.key() < key < position.key()
        return internal_emplace(position, std::forward<Args>(args)...);
      }
    } else if (compare_keys(position.key(), key)) {
      iterator next = position;
      ++next;
      if (next == end() || compare_keys(key, next.key())) {
        // position.key() < key < next.key()
        return internal_emplace(next, std::forward<Args>(args)...);
      }
    } else {
      // position.key() == key
      return position;
    }
  }
  return insert_unique(key, std::forward<Args>(args)...).first;
}

template <typename P>
template <typename InputIterator>
void btree<P>::insert_iterator_unique(InputIterator b, InputIterator e) {
  for (; b != e; ++b) {
    insert_hint_unique(end(), params_type::key(*b), *b);
  }
}

template <typename P>
template <typename ValueType>
auto btree<P>::insert_multi(const key_type &key, ValueType &&v) -> iterator {
  if (empty()) {
    mutable_root() = rightmost_ = new_leaf_root_node(1);
  }

  iterator iter = internal_upper_bound(key, iterator(root(), 0));
  if (!iter.node) {
    iter = end();
  }
  return internal_emplace(iter, std::forward<ValueType>(v));
}

template <typename P>
template <typename ValueType>
auto btree<P>::insert_hint_multi(iterator position, ValueType &&v) -> iterator {
  if (!empty()) {
    const key_type &key = params_type::key(v);
    if (position == end() || !compare_keys(position.key(), key)) {
      iterator prev = position;
      if (position == begin() || !compare_keys(key, (--prev).key())) {
        // prev.key() <= key <= position.key()
        return internal_emplace(position, std::forward<ValueType>(v));
      }
    } else {
      iterator next = position;
      ++next;
      if (next == end() || !compare_keys(next.key(), key)) {
        // position.key() < key <= next.key()
        return internal_emplace(next, std::forward<ValueType>(v));
      }
    }
  }
  return insert_multi(std::forward<ValueType>(v));
}

template <typename P>
template <typename InputIterator>
void btree<P>::insert_iterator_multi(InputIterator b, InputIterator e) {
  for (; b != e; ++b) {
    insert_hint_multi(end(), *b);
  }
}

template <typename P>
auto btree<P>::operator=(const btree &x) -> btree & {
  if (this != &x) {
    clear();

    *mutable_key_comp() = x.key_comp();
    *mutable_allocator() = x.allocator();

    copy_values_in_order(x);
  }
  return *this;
}

template <typename P>
auto btree<P>::erase(iterator iter) -> iterator {
  bool internal_delete = false;
  if (!iter.node->leaf()) {
    // Deletion of a value on an internal node. First, move the largest value
    // from our left child here, then delete that position (in remove_value()
    // below). We can get to the largest value from our left child by
    // decrementing iter.
    iterator internal_iter(iter);
    --iter;
    assert(iter.node->leaf());
    assert(!compare_keys(internal_iter.key(), iter.key()));
    slot_type::move(mutable_allocator(), iter.node->slot(iter.position),
                    internal_iter.node->slot(internal_iter.position));
    internal_delete = true;
  }

  // Delete the key from the leaf.
  iter.node->remove_value(iter.position, mutable_allocator());
  --size_;

  // We want to return the next value after the one we just erased. If we
  // erased from an internal node (internal_delete == true), then the next
  // value is ++(++iter). If we erased from a leaf node (internal_delete ==
  // false) then the next value is ++iter. Note that ++iter may point to an
  // internal node and the value in the internal node may move to a leaf node
  // (iter.node) when rebalancing is performed at the leaf level.

  // Merge/rebalance as we walk back up the tree.
  iterator res(iter);
  for (;;) {
    if (iter.node == root()) {
      try_shrink();
      if (empty()) {
        return end();
      }
      break;
    }
    if (iter.node->count() >= kMinNodeValues) {
      break;
    }
    bool merged = try_merge_or_rebalance(&iter);
    if (iter.node->leaf()) {
      res = iter;
    }
    if (!merged) {
      break;
    }
    iter.node = iter.node->parent();
  }

  // Adjust our return value. If we're pointing at the end of a node, advance
  // the iterator.
  if (res.position == res.node->count()) {
    res.position = res.node->count() - 1;
    ++res;
  }
  // If we erased from an internal node, advance the iterator.
  if (internal_delete) {
    ++res;
  }
  return res;
}

template <typename P>
int btree<P>::erase(iterator begin, iterator end) {
  int count = std::distance(begin, end);
  for (int i = 0; i < count; i++) {
    begin = erase(begin);
  }
  return count;
}

template <typename P> template <typename K>
int btree<P>::erase_unique(const K &key) {
  iterator iter = internal_find_unique(key, iterator(root(), 0));
  if (!iter.node) {
    // The key doesn't exist in the tree, return nothing done.
    return 0;
  }
  erase(iter);
  return 1;
}

template <typename P> template <typename K>
int btree<P>::erase_multi(const K &key) {
  iterator begin = internal_lower_bound(key, iterator(root(), 0));
  if (!begin.node) {
    // The key doesn't exist in the tree, return nothing done.
    return 0;
  }
  // Delete all of the keys between begin and upper_bound(key).
  iterator end = internal_end(
      internal_upper_bound(key, iterator(root(), 0)));
  return erase(begin, end);
}

template <typename P>
void btree<P>::clear() {
  if (root() != nullptr) {
    internal_clear(root());
  }
  mutable_root() = nullptr;
  rightmost_ = nullptr;
  size_ = 0;
}

template <typename P>
void btree<P>::swap(btree &x) {
  using std::swap;
  swap(root_, x.root_);
  swap(rightmost_, x.rightmost_);
  swap(size_, x.size_);
}

template <typename P>
void btree<P>::verify() const {
  if (root() != nullptr) {
    S2_CHECK_EQ(size(), internal_verify(root(), nullptr, nullptr));
    S2_CHECK_EQ(leftmost(), (++const_iterator(root(), -1)).node);
    S2_CHECK_EQ(rightmost_, (--const_iterator(root(), root()->count())).node);
    S2_CHECK(leftmost()->leaf());
    S2_CHECK(rightmost_->leaf());
  } else {
    S2_CHECK_EQ(size(), 0);
    S2_CHECK(leftmost() == nullptr);
    S2_CHECK(rightmost_ == nullptr);
  }
}

template <typename P>
void btree<P>::rebalance_or_split(iterator *iter) {
  node_type *&node = iter->node;
  int &insert_position = iter->position;
  assert(node->count() == node->max_count());

  // First try to make room on the node by rebalancing.
  node_type *parent = node->parent();
  if (node != root()) {
    if (node->position() > 0) {
      // Try rebalancing with our left sibling.
      node_type *left = parent->child(node->position() - 1);
      if (left->count() < left->max_count()) {
        // We bias rebalancing based on the position being inserted. If we're
        // inserting at the end of the right node then we bias rebalancing to
        // fill up the left node.
        int to_move = (left->max_count() - left->count()) /
            (1 + (insert_position < left->max_count()));
        to_move = std::max(1, to_move);

        if (((insert_position - to_move) >= 0) ||
            ((left->count() + to_move) < left->max_count())) {
          left->rebalance_right_to_left(to_move, node, mutable_allocator());

          assert(node->max_count() - node->count() == to_move);
          insert_position = insert_position - to_move;
          if (insert_position < 0) {
            insert_position = insert_position + left->count() + 1;
            node = left;
          }

          assert(node->count() < node->max_count());
          return;
        }
      }
    }

    if (node->position() < parent->count()) {
      // Try rebalancing with our right sibling.
      node_type *right = parent->child(node->position() + 1);
      if (right->count() < right->max_count()) {
        // We bias rebalancing based on the position being inserted. If we're
        // inserting at the beginning of the left node then we bias rebalancing
        // to fill up the right node.
        int to_move = (right->max_count() - right->count()) /
            (1 + (insert_position > 0));
        to_move = std::max(1, to_move);

        if ((insert_position <= (node->count() - to_move)) ||
            ((right->count() + to_move) < right->max_count())) {
          node->rebalance_left_to_right(to_move, right, mutable_allocator());

          if (insert_position > node->count()) {
            insert_position = insert_position - node->count() - 1;
            node = right;
          }

          assert(node->count() < node->max_count());
          return;
        }
      }
    }

    // Rebalancing failed, make sure there is room on the parent node for a new
    // value.
    if (parent->count() == parent->max_count()) {
      iterator parent_iter(node->parent(), node->position());
      rebalance_or_split(&parent_iter);
    }
  } else {
    // Rebalancing not possible because this is the root node.
    // Create a new root node and set the current root node as the child of the
    // new root.
    parent = new_internal_node(parent);
    parent->init_child(0, root());
    mutable_root() = parent;
    // If the former root was a leaf node, then it's now the rightmost node.
    assert(!parent->child(0)->leaf() || parent->child(0) == rightmost_);
  }

  // Split the node.
  node_type *split_node;
  if (node->leaf()) {
    split_node = new_leaf_node(parent);
    node->split(insert_position, split_node, mutable_allocator());
    if (rightmost_ == node) rightmost_ = split_node;
  } else {
    split_node = new_internal_node(parent);
    node->split(insert_position, split_node, mutable_allocator());
  }

  if (insert_position > node->count()) {
    insert_position = insert_position - node->count() - 1;
    node = split_node;
  }
}

template <typename P>
void btree<P>::merge_nodes(node_type *left, node_type *right) {
  left->merge(right, mutable_allocator());
  if (right->leaf()) {
    if (rightmost_ == right) rightmost_ = left;
    delete_leaf_node(right);
  } else {
    delete_internal_node(right);
  }
}

template <typename P>
bool btree<P>::try_merge_or_rebalance(iterator *iter) {
  node_type *parent = iter->node->parent();
  if (iter->node->position() > 0) {
    // Try merging with our left sibling.
    node_type *left = parent->child(iter->node->position() - 1);
    if ((1 + left->count() + iter->node->count()) <= left->max_count()) {
      iter->position += 1 + left->count();
      merge_nodes(left, iter->node);
      iter->node = left;
      return true;
    }
  }
  if (iter->node->position() < parent->count()) {
    // Try merging with our right sibling.
    node_type *right = parent->child(iter->node->position() + 1);
    if ((1 + iter->node->count() + right->count()) <= right->max_count()) {
      merge_nodes(iter->node, right);
      return true;
    }
    // Try rebalancing with our right sibling. We don't perform rebalancing if
    // we deleted the first element from iter->node and the node is not
    // empty. This is a small optimization for the common pattern of deleting
    // from the front of the tree.
    if ((right->count() > kMinNodeValues) &&
        ((iter->node->count() == 0) ||
         (iter->position > 0))) {
      int to_move = (right->count() - iter->node->count()) / 2;
      to_move = std::min(to_move, right->count() - 1);
      iter->node->rebalance_right_to_left(to_move, right, mutable_allocator());
      return false;
    }
  }
  if (iter->node->position() > 0) {
    // Try rebalancing with our left sibling. We don't perform rebalancing if
    // we deleted the last element from iter->node and the node is not
    // empty. This is a small optimization for the common pattern of deleting
    // from the back of the tree.
    node_type *left = parent->child(iter->node->position() - 1);
    if ((left->count() > kMinNodeValues) &&
        ((iter->node->count() == 0) ||
         (iter->position < iter->node->count()))) {
      int to_move = (left->count() - iter->node->count()) / 2;
      to_move = std::min(to_move, left->count() - 1);
      left->rebalance_left_to_right(to_move, iter->node, mutable_allocator());
      iter->position += to_move;
      return false;
    }
  }
  return false;
}

template <typename P>
void btree<P>::try_shrink() {
  if (root()->count() > 0) {
    return;
  }
  // Deleted the last item on the root node, shrink the height of the tree.
  if (root()->leaf()) {
    assert(size() == 0);
    delete_leaf_node(root());
    mutable_root() = nullptr;
    rightmost_ = nullptr;
  } else {
    node_type *child = root()->child(0);
    child->make_root();
    delete_internal_node(root());
    mutable_root() = child;
  }
}

template <typename P> template <typename IterType>
inline IterType btree<P>::internal_last(IterType iter) {
  while (iter.node && iter.position == iter.node->count()) {
    iter.position = iter.node->position();
    iter.node = iter.node->parent();
    if (iter.node->leaf()) {
      iter.node = nullptr;
    }
  }
  return iter;
}

template <typename P>
template <typename... Args>
inline auto btree<P>::internal_emplace(iterator iter, Args &&... args)
    -> iterator {
  if (!iter.node->leaf()) {
    // We can't insert on an internal node. Instead, we'll insert after the
    // previous value which is guaranteed to be on a leaf node.
    --iter;
    ++iter.position;
  }
  if (iter.node->count() == iter.node->max_count()) {
    // Make room in the leaf for the new item.
    if (iter.node->max_count() < kNodeValues) {
      // Insertion into the root where the root is smaller that the full node
      // size. Simply grow the size of the root node.
      assert(iter.node == root());
      iter.node = new_leaf_root_node(
          std::min<int>(kNodeValues, 2 * iter.node->max_count()));
      iter.node->swap(root(), mutable_allocator());
      delete_leaf_node(root());
      mutable_root() = iter.node;
      rightmost_ = iter.node;
    } else {
      rebalance_or_split(&iter);
    }
  }
  iter.node->emplace_value(iter.position, mutable_allocator(),
                           std::forward<Args>(args)...);
  ++size_;
  return iter;
}

template <typename P> template <typename K, typename IterType>
inline std::pair<IterType, int> btree<P>::internal_locate(
    const K &key, IterType iter) const {
  return is_key_compare_to::value ? internal_locate_compare_to(key, iter)
                                  : internal_locate_plain_compare(key, iter);
}

template <typename P> template <typename K, typename IterType>
inline std::pair<IterType, int> btree<P>::internal_locate_plain_compare(
    const K &key, IterType iter) const {
  for (;;) {
    iter.position = iter.node->lower_bound(key, key_comp());
    if (iter.node->leaf()) {
      break;
    }
    iter.node = iter.node->child(iter.position);
  }
  return std::make_pair(iter, 0);
}

template <typename P> template <typename K, typename IterType>
inline std::pair<IterType, int> btree<P>::internal_locate_compare_to(
    const K &key, IterType iter) const {
  for (;;) {
    int res = iter.node->lower_bound(key, key_comp());
    iter.position = res & kMatchMask;
    if (res & kExactMatch) {
      return std::make_pair(iter, static_cast<int>(kExactMatch));
    }
    if (iter.node->leaf()) {
      break;
    }
    iter.node = iter.node->child(iter.position);
  }
  return std::make_pair(iter, -kExactMatch);
}

template <typename P> template <typename K, typename IterType>
IterType btree<P>::internal_lower_bound(
    const K &key, IterType iter) const {
  const_lookup_key_reference<K> lookup_key(key);
  if (iter.node) {
    for (;;) {
      iter.position =
          iter.node->lower_bound(lookup_key, key_comp()) & kMatchMask;
      if (iter.node->leaf()) {
        break;
      }
      iter.node = iter.node->child(iter.position);
    }
    iter = internal_last(iter);
  }
  return iter;
}

template <typename P> template <typename K, typename IterType>
IterType btree<P>::internal_upper_bound(
    const K &key, IterType iter) const {
  const_lookup_key_reference<K> lookup_key(key);
  if (iter.node) {
    for (;;) {
      iter.position = iter.node->upper_bound(lookup_key, key_comp());
      if (iter.node->leaf()) {
        break;
      }
      iter.node = iter.node->child(iter.position);
    }
    iter = internal_last(iter);
  }
  return iter;
}

template <typename P> template <typename K, typename IterType>
IterType btree<P>::internal_find_unique(
    const K &key, IterType iter) const {
  const_lookup_key_reference<K> lookup_key(key);
  if (iter.node) {
    std::pair<IterType, int> res = internal_locate(lookup_key, iter);
    if (res.second == kExactMatch) {
      return res.first;
    }
    if (!res.second) {
      iter = internal_last(res.first);
      if (iter.node && !compare_keys(lookup_key, iter.key())) {
        return iter;
      }
    }
  }
  return IterType(nullptr, 0);
}

template <typename P> template <typename K, typename IterType>
IterType btree<P>::internal_find_multi(
    const K &key, IterType iter) const {
  const_lookup_key_reference<K> lookup_key(key);
  if (iter.node) {
    iter = internal_lower_bound(lookup_key, iter);
    if (iter.node) {
      iter = internal_last(iter);
      if (iter.node && !compare_keys(lookup_key, iter.key())) {
        return iter;
      }
    }
  }
  return IterType(nullptr, 0);
}

template <typename P>
void btree<P>::internal_clear(node_type *node) {
  if (!node->leaf()) {
    for (int i = 0; i <= node->count(); ++i) {
      internal_clear(node->child(i));
    }
    delete_internal_node(node);
  } else {
    delete_leaf_node(node);
  }
}

template <typename P>
int btree<P>::internal_verify(
    const node_type *node, const key_type *lo, const key_type *hi) const {
  S2_CHECK_GT(node->count(), 0);
  S2_CHECK_LE(node->count(), node->max_count());
  if (lo) {
    S2_CHECK(!compare_keys(node->key(0), *lo));
  }
  if (hi) {
    S2_CHECK(!compare_keys(*hi, node->key(node->count() - 1)));
  }
  for (int i = 1; i < node->count(); ++i) {
    S2_CHECK(!compare_keys(node->key(i), node->key(i - 1)));
  }
  int count = node->count();
  if (!node->leaf()) {
    for (int i = 0; i <= node->count(); ++i) {
      S2_CHECK(node->child(i) != nullptr);
      S2_CHECK_EQ(node->child(i)->parent(), node);
      S2_CHECK_EQ(node->child(i)->position(), i);
      count += internal_verify(
          node->child(i),
          (i == 0) ? lo : &node->key(i - 1),
          (i == node->count()) ? hi : &node->key(i));
    }
  }
  return count;
}

}  // namespace internal_btree
}  // namespace gtl


#endif  // S2_UTIL_GTL_BTREE_H_
