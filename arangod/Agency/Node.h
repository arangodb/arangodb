////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_NODE_H
#define ARANGOD_CONSENSUS_NODE_H 1

#include "AgencyCommon.h"
#include "Basics/ResultT.h"

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace arangodb::consensus {

enum NodeType { NODE, LEAF };
enum Operation {
  SET,
  INCREMENT,
  DECREMENT,
  PUSH,
  POP,
  PREPEND,
  SHIFT,
  OBSERVE,
  UNOBSERVE,
  ERASE,
  REPLACE,
  READ_LOCK,
  READ_UNLOCK,
  WRITE_LOCK,
  WRITE_UNLOCK,
};

using namespace arangodb::velocypack;

class StoreException : public std::exception {
 public:
  explicit StoreException(std::string const& message) : _message(message) {}
  virtual char const* what() const noexcept override final {
    return _message.c_str();
  }

 private:
  std::string _message;
};

enum NODE_EXCEPTION { PATH_NOT_FOUND };

typedef std::chrono::system_clock::time_point TimePoint;
typedef std::chrono::steady_clock::time_point SteadyTimePoint;

class Store;

class SmallBuffer {
  uint8_t* _start;
  size_t _size;
 public:
  SmallBuffer() noexcept : _start(nullptr), _size(0) {}
  explicit SmallBuffer(size_t size) : SmallBuffer() {
    if (size > 0) {
      _start = new uint8_t[size];
      _size = size;
    }
  }
  explicit SmallBuffer(uint8_t const* data, size_t size) : SmallBuffer(size) {
    memcpy(_start, data, size);
  }
  SmallBuffer(SmallBuffer const& other) 
      : SmallBuffer() {
    if (!other.empty()) {
      _start = new uint8_t[other._size];
      _size = other._size;
      memcpy(_start, other._start, other._size);
    }
  }
  SmallBuffer(SmallBuffer&& other) noexcept
      : _start(other._start), _size(other._size) {
    other._start = nullptr;
    other._size = 0;
  }
  SmallBuffer& operator=(SmallBuffer const& other) {
    if (this != &other) {
      if (!empty()) {
        delete[] _start;
      }
      if (other.empty()) {
        _start = nullptr;
        _size = 0;
      } else {
        _start = new uint8_t[other._size];
        _size = other._size;
        memcpy(_start, other._start, other._size);
      }
    }
    return *this;
  }
  SmallBuffer& operator=(SmallBuffer&& other) noexcept {
    if (this != &other) {
      if (!empty()) {
        delete[] _start;
      }
      _start = other._start;
      other._start = nullptr;
      _size = other._size;
      other._size = 0;
    }
    return *this;
  }
  ~SmallBuffer() {
    delete[] _start;
  }
  uint8_t* data() const { return _start; }
  size_t size() const { return _size; }
  bool empty() const { return _start == nullptr || _size == 0; }
};

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
  typedef std::vector<std::string> PathType;

  /// @brief Child nodes
  typedef std::unordered_map<std::string, std::shared_ptr<Node>> Children;

  /// @brief Construct with name
  explicit Node(std::string const& name);

  /// @brief Copy constructor
  Node(Node const& other);

  /// @brief Move constructor
  Node(Node&& other) noexcept;

  /// @brief Construct with name and introduce to tree under parent
  Node(std::string const& name, Node* parent);

  /// @brief Construct with name and introduce to tree under parent
  Node(std::string const& name, Store* store);

  /// @brief Default dtor
  ~Node();

  /// @brief Get name
  std::string const& name() const;

  /// @brief Get full path
  std::string uri() const;

  /// @brief Assignment operator
  Node& operator=(Node const& node);

  /// @brief Move operator
  Node& operator=(Node&& node) noexcept;

  /// @brief Apply value slice to this node
  Node& operator=(arangodb::velocypack::Slice const&);

  /// @brief Check equality with slice
  bool operator==(arangodb::velocypack::Slice const&) const;

  /// @brief Check equality with slice
  bool operator!=(arangodb::velocypack::Slice const&) const;

  /// @brief Type of this node (LEAF / NODE)
  NodeType type() const;

  /// @brief Get node specified by path vector
  Node& operator()(std::vector<std::string> const& pv);

  /// @brief Get node specified by path vector
  Node const& operator()(std::vector<std::string> const& pv) const;

  /// @brief Get root node
  Node const& root() const;

  /// @brief Get root node
  Node& root();

  /// @brief Dump to ostream
  std::ostream& print(std::ostream&) const;

  /// #brief Get path of this node
  std::string path();

  /// @brief Apply single operation as defined by "op"
  ResultT<std::shared_ptr<Node>> applyOp(arangodb::velocypack::Slice const&);

  /// @brief Apply single slice
  bool applies(arangodb::velocypack::Slice const&);

  /// @brief Return all keys of an object node. Result will be empty for non-objects.
  std::vector<std::string> keys() const;

  /// @brief handle "op" keys in write json
  template <Operation Oper>
  arangodb::ResultT<std::shared_ptr<Node>> handle(arangodb::velocypack::Slice const&);

  /// @brief Create Builder representing this store
  void toBuilder(Builder&, bool showHidden = false) const;

  /// @brief Create Builder representing this store
  VPackBuilder toBuilder() const;

  /// @brief Access children

  /// @brief Access for unit tests:
  void addChild(std::string const& name, std::shared_ptr<Node>& node);

  /// @brief Access children
  Children const& children() const;

  /// @brief Create slice from value
  Slice slice() const;

  /// @brief Get value type
  ValueType valueType() const;

  /// @brief Create JSON representation of this node and below
  std::string toJson() const;

  /// @brief Parent node
  Node const* parent() const;

  /// @brief Part of relative path vector which exists
  std::vector<std::string> exists(std::vector<std::string> const&) const;

  /// @brief Part of relative path which exists
  std::vector<std::string> exists(std::string const&) const;

  /// @brief Part of relative path vector which exists
  bool has(std::vector<std::string> const&) const;

  /// @brief Part of relative path which exists
  bool has(std::string const&) const;

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

  /**
   * @brief Set expiry for this node
   * @param Time point of expiry
   */
  void timeToLive(TimePoint const& ttl);

  /// @brief accessor to Node object
  /// @return  second is true if url exists, first populated if second true
  std::pair<Node const&, bool> hasAsNode(std::string const&) const;

  /// @brief accessor to Node object
  /// @return  second is true if url exists, first populated if second true
  std::pair<Node&, bool> hasAsWritableNode(std::string const&);

  /// @brief accessor to Node's type
  /// @return  second is true if url exists, first populated if second true
  std::pair<NodeType, bool> hasAsType(std::string const&) const;

  /// @brief accessor to Node's Slice value
  /// @return  second is true if url exists, first populated if second true
  std::pair<Slice, bool> hasAsSlice(std::string const&) const;

  /// @brief accessor to Node's uint64_t value
  /// @return  second is true if url exists, first populated if second true
  std::pair<uint64_t, bool> hasAsUInt(std::string const&) const;

  /// @brief accessor to Node's bool value
  /// @return  second is true if url exists, first populated if second true
  std::pair<bool, bool> hasAsBool(std::string const&) const;

  /// @brief accessor to Node's std::string value
  /// @return  second is true if url exists, first populated if second true
  std::pair<std::string, bool> hasAsString(std::string const&) const;

  /// @brief accessor to Node's _children
  /// @return  second is true if url exists, first populated if second true
  std::pair<Children const&, bool> hasAsChildren(std::string const&) const;

  /// @brief accessor to Node then write to builder
  /// @return  second is true if url exists, first is ignored
  std::pair<void*, bool> hasAsBuilder(std::string const&, Builder&,
                                      bool showHidden = false) const;

  /// @brief accessor to Node's value as a Builder object
  /// @return  second is true if url exists, first populated if second true
  std::pair<Builder, bool> hasAsBuilder(std::string const&) const;

  /// @brief accessor to Node's Array
  /// @return  second is true if url exists, first populated if second true
  std::pair<Slice, bool> hasAsArray(std::string const&) const;

  // These two operator() functions could be "protected" once
  //  unit tests updated.
  //
  /// @brief Get node specified by path string
  Node& operator()(std::string const& path);

  /// @brief Get node specified by path string
  Node const& operator()(std::string const& path) const;

  /// @brief Get string value (throws if type NODE or if conversion fails)
  std::string getString() const;

  /// @brief Get array value
  Slice getArray() const;

  /// @brief Get insigned value (throws if type NODE or if conversion fails)
  uint64_t getUInt() const;

  /// @brief Get node specified by path string, always throw if not there
  Node const& get(std::string const& path) const;

  /// @brief Get integer value (throws if type NODE or if conversion fails)
  int64_t getInt() const;

  /// @brief Get bool value (throws if type NODE or if conversion fails)
  bool getBool() const;

  /// @brief Get double value (throws if type NODE or if conversion fails)
  double getDouble() const;

  template<typename T>
  auto getNumberUnlessExpiredWithDefault() -> T {
    if (ADB_LIKELY(!lifetimeExpired())) {
      try {
        return this->slice().getNumber<T>();
      } catch (...) {
      }
    }

    return T{0};
  }

  static auto getIntWithDefault(Slice slice, std::string_view key, std::int64_t def) -> std::int64_t;

  bool isReadLockable(const VPackStringRef& by) const;
  bool isWriteLockable(const VPackStringRef& by) const;

  /// @brief Clear key value store
  void clear();

  // @brief Helper function to return static instance of dummy node below
  static Node const& dummyNode() {
    return _dummyNode;
  }
 
 private:
  
  bool isReadUnlockable(const VPackStringRef& by) const;
  bool isWriteUnlockable(const VPackStringRef& by) const;
  
  /// @brief  Remove child by name
  /// @return shared pointer to removed child
  arangodb::ResultT<std::shared_ptr<Node>> removeChild(std::string const& key);

  /// @brief Get root store if it exists:
  Store* getRootStore() const;

  /// @brief Remove me from tree, if not root node, clear else.
  /// @return If not root node, shared pointer copy to this node is returned
  ///         to control life time by caller; else nullptr.
  arangodb::ResultT<std::shared_ptr<Node>> deleteMe();

  // @brief check lifetime expiry
  bool lifetimeExpired() const;

  /// @brief Add time to live entry
  bool addTimeToLive(std::chrono::time_point<std::chrono::system_clock> const& tp);

  /// @brief Remove time to live entry
  bool removeTimeToLive();

  void rebuildVecBuf() const;

  std::string _nodeName;                ///< @brief my name
  Node* _parent;                        ///< @brief parent
  Store* _store;                        ///< @brief Store
  mutable std::unique_ptr<Children> _children;  ///< @brief child nodes
  TimePoint _ttl;                       ///< @brief my expiry
  std::unique_ptr<std::vector<SmallBuffer>> _value;  ///< @brief my value
  mutable std::unique_ptr<SmallBuffer> _vecBuf;
  mutable bool _vecBufDirty;
  bool _isArray;
  static Children const dummyChildren;
  static Node const _dummyNode;

};

inline std::ostream& operator<<(std::ostream& o, Node const& n) {
  return n.print(o);
}

}  // namespace arangodb::consensus

#endif
