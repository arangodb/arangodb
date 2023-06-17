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

#include "Basics/ResultT.h"
#include "Agency/AgencyCommon.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <cstring>
#include <chrono>

#include <vector>
#include <unordered_map>

#include <velocypack/String.h>
#include <velocypack/Slice.h>

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
class SmallBuffer;

/// @brief Simple tree implementation

/// Any node may either be a branch or a leaf.
/// Any leaf either represents an array or an element (_isArray variable).
/// Nodes are are always constructed as element and can become an array through
/// assignment operator.
/// toBuilder(Builder&) will create a _vecBuf, when needed as a means to
/// optimization by avoiding to build it before necessary.
class Node final {
  /// @brief Access private methods
  friend class Store;

 public:
  /// @brief Slash-segmented path
  using PathType = std::vector<std::string>;

  /// @brief Child nodes
  using Children = std::unordered_map<std::string, std::shared_ptr<Node>>;

  using Array = std::vector<velocypack::String>;

  /// @brief Split strings by forward slashes, omitting empty strings,
  /// and ignoring multiple subsequent forward slashes
  static std::vector<std::string> split(std::string_view str);

  Node() = default;
  Node(Node const& other) = default;
  Node(Node&& other) noexcept = default;
  Node& operator=(Node const& node) = default;
  Node& operator=(Node&& node) noexcept = default;

  /// @brief Default dtor
  ~Node();

  /// @brief Apply value slice to this node
  Node& operator=(arangodb::velocypack::Slice const&);

  /// @brief Check equality with slice
  bool operator==(arangodb::velocypack::Slice const&) const;

  /// @brief Check equality with slice
  bool operator!=(arangodb::velocypack::Slice const&) const;

  /// @brief Get node specified by path vector
  Node const* get(std::vector<std::string> const& pv) const;

  /// @brief Get node specified by path
  Node const* get(
      std::shared_ptr<cluster::paths::Path const> const& path) const;

  /// @brief Dump to ostream
  std::ostream& print(std::ostream&) const;

  /// @brief Apply single operation as defined by "op"
  ResultT<std::shared_ptr<Node>> applyOp(arangodb::velocypack::Slice);

  /// @brief Apply single slice
  bool applies(arangodb::velocypack::Slice);

  /// @brief Return all keys of an object node. Result will be empty for
  /// non-objects.
  std::vector<std::string> keys() const;

  /// @brief handle "op" keys in write json
  template<Operation Oper>
  arangodb::ResultT<std::shared_ptr<Node>> handle(
      arangodb::velocypack::Slice const&);

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

  /**
   * @brief Set expiry for this node
   * @param Time point of expiry
   */
  void timeToLive(TimePoint const& ttl);

  /// @brief accessor to Node object
  /// @return  returns nullptr if not found or type doesn't match
  Node const* hasAsNode(std::string const&) const noexcept;

  /// @brief accessor to Node's Slice value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<velocypack::Slice> hasAsSlice(
      std::string const&) const noexcept;

  /// @brief accessor to Node's uint64_t value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<uint64_t> hasAsUInt(std::string const&) const noexcept;

  /// @brief accessor to Node's bool value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<bool> hasAsBool(std::string const&) const noexcept;

  /// @brief accessor to Node's std::string value
  /// @return  returns nullopt if not found or type doesn't match
  std::optional<std::string> hasAsString(std::string const&) const;

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

  // These two operator() functions could be "protected" once
  //  unit tests updated.
  //
  /// @brief Get node specified by path string
  Node& getOrCreate(std::string_view path);

  /// @brief Get node specified by path string
  Node const* get(std::string_view path) const;

  /// @brief Get string value (throws if type NODE or if conversion fails)
  std::optional<std::string> getString() const;

  /// @brief Get string value (throws if type NODE or if conversion fails)
  std::optional<std::string_view> getStringView() const;

  /// @brief Get array value
  Node::Array const* getArray() const;

  /// @brief Get unsigned value (throws if type NODE or if conversion fails)
  std::optional<uint64_t> getUInt() const noexcept;

  /// @brief Get integer value (throws if type NODE or if conversion fails)
  std::optional<int64_t> getInt() const noexcept;

  /// @brief Get bool value (throws if type NODE or if conversion fails)
  std::optional<bool> getBool() const noexcept;

  /// @brief Get double value (throws if type NODE or if conversion fails)
  std::optional<double> getDouble() const noexcept;

  VPackSlice slice() const noexcept;

  bool isLeaveNode() const noexcept { return !isObject(); }

  static auto getIntWithDefault(velocypack::Slice slice, std::string_view key,
                                std::int64_t def) -> std::int64_t;

  bool isReadLockable(std::string_view by) const;
  bool isWriteLockable(std::string_view by) const;
  bool isWriteLocked(std::string_view by) const {
    return isWriteUnlockable(by);
  }
  bool isReadLocked(std::string_view by) const { return isReadUnlockable(by); }
  bool isReadUnlockable(std::string_view by) const;
  bool isWriteUnlockable(std::string_view by) const;

  /// @brief Clear key value store
  void clear();

  // @brief Helper function to return static instance of dummy node below
  static Node const& dummyNode() { return _dummyNode; }

 private:
  /// @brief  Remove child by name
  /// @return shared pointer to removed child
  arangodb::ResultT<std::shared_ptr<Node>> removeChild(std::string const& key);

  std::variant<Children, Array, velocypack::String> _value;

  static Children const dummyChildren;
  static Node const _dummyNode;
};

inline std::ostream& operator<<(std::ostream& o, Node const& n) {
  return n.print(o);
}

}  // namespace arangodb::consensus
