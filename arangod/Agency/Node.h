////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Agency/AgencyCommon.h"
#include "Basics/ResultT.h"
#include "Containers/ImmerMemoryPolicy.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <velocypack/Slice.h>
#include <velocypack/String.h>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of
// data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift
// intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#include <immer/map.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

namespace arangodb::cluster::paths {
class Path;
}

namespace arangodb::consensus {

enum Operation {
  SET,
  INCREMENT,
  DECREMENT,
  PUSH,
  POP,
  PREPEND,
  SHIFT,
  ERASE,
  REPLACE,
  READ_LOCK,
  READ_UNLOCK,
  WRITE_LOCK,
  WRITE_UNLOCK,
  PUSH_QUEUE,
};

class Store;

class Node;

/// @brief Simple tree implementation
using NodePtr = std::shared_ptr<Node const>;

/// Any node may either be a branch or a leaf.
/// Any leaf either represents an array or an element (_isArray variable).
/// Nodes are are always constructed as element and can become an array through
/// assignment operator.
/// toBuilder(Builder&) will create a _vecBuf, when needed as a means to
/// optimization by avoiding to build it before necessary.
class Node : public std::enable_shared_from_this<Node> {
 public:
  /// @brief Slash-segmented path
  using PathType = std::vector<std::string>;
  using PathTypeView = std::span<std::string const>;

 private:
  template<typename T>
  struct AccountingAllocator : std::allocator<T> {
    template<typename U>
    AccountingAllocator(AccountingAllocator<U> const&) {}
    AccountingAllocator() = default;

    T* allocate(std::size_t n) {
      auto mem = std::allocator<T>::allocate(n);
      Node::increaseMemoryUsage(sizeof(T) * n);
      return mem;
    }

    void deallocate(T* p, std::size_t n) {
      std::allocator<T>::deallocate(p, n);
      Node::decreaseMemoryUsage(sizeof(T) * n);
    }

    template<typename U>
    struct rebind {
      using type = AccountingAllocator<U>;
    };
  };

  template<typename Base>
  struct AccountingHeap : Base {
    template<typename... Tags>
    static void* allocate(std::size_t size, Tags... tags) {
      auto* mem = Base::allocate(size, tags...);
      Node::increaseMemoryUsage(size);
      return mem;
    }

    template<typename... Tags>
    static void deallocate(std::size_t size, void* data, Tags... tags) {
      Base::deallocate(size, data, tags...);
      Node::decreaseMemoryUsage(size);
    }
  };

  using AccountingMemoryPolicy = ::immer::memory_policy<
      AccountingHeap<arangodb::immer::arango_heap_policy>,
      ::immer::default_refcount_policy, ::immer::default_lock_policy>;

  static void increaseMemoryUsage(std::size_t) noexcept;
  static void decreaseMemoryUsage(std::size_t) noexcept;
  static std::atomic<std::size_t> memoryUsage;

 public:
  using allocator_type = AccountingAllocator<Node>;

  static std::size_t getMemoryUsage() noexcept { return memoryUsage; }
  static void toPrometheus(std::string& result, std::string_view globals,
                           bool ensureWhitespace);

  // If you want those types to be accounted for as well, please do so.
  // But be aware that this will change the type and the interface and you have
  // to modify a lot of code to make this work. You have been warned.
  using StringType = std::basic_string<char, std::char_traits<char>
                                       /*, accounting_allocator<char>*/>;
  using VPackStringType = velocypack::BasicString<AccountingAllocator<uint8_t>>;

  struct TransparentHash {
    using is_transparent = void;
    template<typename Traits, typename Alloc>
    auto operator()(std::basic_string<char, Traits, Alloc> const& str) {
      return std::hash<std::string_view>{}(str);
    }
    auto operator()(std::string_view str) {
      return std::hash<std::string_view>{}(str);
    }
  };

  struct TransparentEqual {
    using is_transparent = void;
    template<typename T, typename U>
    bool operator()(T const& t, U const& u) {
      return t == u;
    }

    template<typename Chars, typename Traits, typename Alloc1, typename Alloc2>
    auto operator()(std::basic_string<Chars, Traits, Alloc1> const& str,
                    std::basic_string<Chars, Traits, Alloc2> const& other) {
      return std::string_view{str} == other;
    }
  };

  using Children = ::immer::map<StringType, NodePtr, TransparentHash,
                                TransparentEqual, AccountingMemoryPolicy>;

  using Array = ::immer::flex_vector<VPackStringType, AccountingMemoryPolicy>;

  using VariantType = std::variant<Children, Array, VPackStringType>;

  /// @brief Split strings by forward slashes, omitting empty strings,
  /// and ignoring multiple subsequent forward slashes
  static std::vector<std::string> split(std::string_view str);

  /// @brief Default dtor
  ~Node() = default;

  /// @brief Check equality with slice
  bool operator==(arangodb::velocypack::Slice const&) const;

  /// @brief Get node specified by path vector. Returns nullptr if the node does
  /// not exist.
  NodePtr get(PathTypeView pv) const;
  NodePtr get(std::initializer_list<std::string> const& pv) const;
  NodePtr get(std::string_view path) const;
  NodePtr get(std::shared_ptr<cluster::paths::Path const> const& path) const;

  /// @brief Dump to ostream
  std::ostream& print(std::ostream&) const;

  /// @brief Return all keys of an object node. Result will be empty for
  /// non-objects.
  std::vector<std::string> keys() const;

  /// @brief Create Builder representing this store
  void toBuilder(velocypack::Builder&, bool showHidden = false) const;

  /// @brief Create Builder representing this store
  velocypack::Builder toBuilder() const;

  /// @brief Access children
  Children const& children() const;

  /// @brief Create JSON representation of this node and below
  std::string toJson() const;

  /// @brief Part of relative path vector which exists
  std::vector<std::string> exists(std::vector<std::string> const& rel) const;

  /// @brief Part of relative path which exists
  std::vector<std::string> exists(std::string_view rel) const;

  /// @brief Part of relative path vector which exists
  bool has(std::vector<std::string> const& rel) const;

  /// @brief Part of relative path which exists
  bool has(std::string_view rel) const;

  /// @brief Is Int
  bool isInt() const;

  /// @brief Is UInt
  bool isUInt() const;

  /// @brief Is number
  bool isNumber() const;

  /// @brief Is boolean
  bool isBool() const;

  /// @brief Is double
  bool isDouble() const;

  /// @brief Is string
  bool isString() const;

  /// @brief Is array
  bool isArray() const;

  /// @brief Is object
  bool isObject() const;

  /// @brief Is null
  bool isNull() const;

  /// @brief accessor to Node's uint64_t value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<uint64_t> hasAsUInt(std::string const&) const noexcept;

  /// @brief accessor to Node's bool value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<bool> hasAsBool(std::string const&) const noexcept;

  /// @brief accessor to Node's std::string value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<std::string> hasAsString(std::string const&) const;

  /// @brief accessor to Node's std::string value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<std::string_view> hasAsStringView(std::string const&) const;

  /// @brief accessor to Node's _children
  /// @return  returns nullptr if not found or type doesn't match
  Children const* hasAsChildren(std::string const&) const;

  /// @brief accessor to Node then write to builder
  /// @return  returns true if url exists
  [[nodiscard]] bool hasAsBuilder(std::string const&, velocypack::Builder&,
                                  bool showHidden = false) const;

  /// @brief accessor to Node's value as a Builder object
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<velocypack::Builder> hasAsBuilder(std::string const&) const;

  /// @brief accessor to Node's Array
  /// @return  returns nullopt if not found or type doesn't match
  Array const* hasAsArray(std::string const&) const;

  /// @brief Get string value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<std::string> getString() const;

  /// @brief Get string value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<std::string_view> getStringView() const;

  /// @brief Get array value, or nullopt if wrong type
  Node::Array const* getArray() const;

  /// @brief Get unsigned value, or nullopt if wrong type
  std::optional<uint64_t> getUInt() const noexcept;

  /// @brief Get integer value, or nullopt if wrong type
  std::optional<int64_t> getInt() const noexcept;

  /// @brief Get bool value, or nullopt if wrong type
  std::optional<bool> getBool() const noexcept;

  /// @brief Get double value, or nullopt if wrong type
  std::optional<double> getDouble() const noexcept;

  /// @brief Get numeric value, or nullopt if wrong type
  template<typename T>
  std::optional<T> getNumber() const noexcept {
    try {
      return slice().getNumber<T>();
    } catch (...) {
      return std::nullopt;
    }
  }

  velocypack::ValueType getVelocyPackValueType() const noexcept;

  bool isReadLockable(std::string_view by) const;
  bool isWriteLockable(std::string_view by) const;
  bool isWriteLocked(std::string_view by) const {
    return isWriteUnlockable(by);
  }
  bool isReadLocked(std::string_view by) const { return isReadUnlockable(by); }
  bool isReadUnlockable(std::string_view by) const;
  bool isWriteUnlockable(std::string_view by) const;

  [[nodiscard]] static ResultT<NodePtr> applyOp(Node const* target,
                                                VPackSlice slice);
  [[nodiscard]] ResultT<NodePtr> applyOp(std::string_view path,
                                         VPackSlice slice) const;
  [[nodiscard]] NodePtr placeAt(std::string_view path, NodePtr) const;
  [[nodiscard]] NodePtr placeAt(PathTypeView path, NodePtr) const;
  [[nodiscard]] NodePtr placeAt(std::initializer_list<std::string> const& path,
                                NodePtr) const;

  template<Operation Op>
  [[nodiscard]] NodePtr perform(std::string_view path,
                                velocypack::Slice op) const {
    auto res = Node::handle<Op>(get(path).get(), op);
    if (res.fail()) {
      return res;
    }
    return placeAt(path, res.get());
  }
  template<Operation Op>
  [[nodiscard]] ResultT<NodePtr> perform(PathTypeView path,
                                         velocypack::Slice op) const {
    auto res = Node::handle<Op>(get(path).get(), op);
    if (res.fail()) {
      return res;
    }
    return placeAt(path, res.get());
  }
  template<Operation Op>
  [[nodiscard]] ResultT<NodePtr> perform(
      std::initializer_list<std::string> const& path,
      velocypack::Slice op) const {
    return perform<Op>(PathTypeView{path.begin(), path.end()}, op);
  }

  template<typename T>
  [[nodiscard]] NodePtr placeAt(std::string_view path, T&& value) const {
    return placeAt(path, Node::create(std::forward<T>(value)));
  }
  template<typename T>
  [[nodiscard]] NodePtr placeAt(PathTypeView path, T&& value) const {
    return placeAt(path, Node::create(std::forward<T>(value)));
  }
  template<typename T>
  [[nodiscard]] NodePtr placeAt(std::initializer_list<std::string> const& path,
                                T&& value) const {
    return placeAt(path, Node::create(std::forward<T>(value)));
  }

  [[nodiscard]] NodePtr applies(std::string_view path, VPackSlice) const;

  // Get rid of this constructor mess
  [[nodiscard]] static NodePtr create(VPackSlice);
  [[nodiscard]] static NodePtr create();
  [[nodiscard]] static NodePtr create(VariantType);
  [[nodiscard]] static NodePtr create(Array);
  [[nodiscard]] static NodePtr create(Children);
  template<typename T>
  [[nodiscard]] static NodePtr create(T&& value) {
    velocypack::Builder builder;
    builder.add(velocypack::Value(value));
    return Node::create(builder.slice());
  }

  /// @brief handle "op" keys in write json
  template<Operation Oper>
  [[nodiscard]] static arangodb::ResultT<NodePtr> handle(
      Node const* target, arangodb::velocypack::Slice const&);

  static NodePtr trueValue();
  static NodePtr falseValue();
  static NodePtr nullValue();
  static NodePtr emptyObjectValue();
  static NodePtr emptyArrayValue();

  Node(Node const& other) = delete;
  Node(Node&& other) noexcept = delete;
  Node& operator=(Node const& node) = delete;
  Node& operator=(Node&& node) noexcept = delete;

 private:
  VPackSlice slice() const noexcept;

  template<typename... Args>
  static NodePtr allocateNode(Args&&...);

  Node() = default;
  template<typename... Args>
  explicit Node(Args&&... args) : _value(std::forward<Args>(args)...) {}

  VariantType _value;

  static Children const dummyChildren;
};

inline std::ostream& operator<<(std::ostream& o, Node const& n) {
  return n.print(o);
}

}  // namespace arangodb::consensus
