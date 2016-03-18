////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef __ARANGODB_CONSENSUS_STORE__
#define __ARANGODB_CONSENSUS_STORE__

#include "AgencyCommon.h"

#include <type_traits>
#include <utility>
#include <typeinfo>
#include <string>
#include <cassert>
#include <map>
#include <vector>
#include <list>
#include <memory>
#include <cstdint>

#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>
#include <Basics/Thread.h>
#include <Basics/ConditionVariable.h>
#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

enum NodeType {NODE, LEAF};

using namespace arangodb::velocypack;

class StoreException : public std::exception {
public:
  StoreException(std::string const& message) : _message(message) {}
  virtual char const* what() const noexcept { return _message.c_str(); }
private:
  std::string _message;
};

enum NODE_EXCEPTION {PATH_NOT_FOUND};

/// @brief Simple tree implementation
class Node {
  
public:

  typedef std::vector<std::string> PathType;
  typedef std::map<std::string, std::shared_ptr<Node>> Children;
  typedef std::chrono::system_clock::time_point TimePoint;
  typedef std::map<TimePoint, std::shared_ptr<Node>> TimeTable;
  
  /// @brief Construct with name
  Node (std::string const& name);

  /// @brief Construct with name and introduce to tree under parent
  Node (std::string const& name, Node* parent);

  /// @brief Default dtor
  virtual ~Node ();

  /// @brief Get name 
  std::string const& name() const;

  /// @brief Apply rhs to this node (deep copy of rhs)
  Node& operator= (Node const& node);

  /// @brief Apply value slice to this node
  Node& operator= (arangodb::velocypack::Slice const&);

  /// @brief Check equality with slice
  bool operator== (arangodb::velocypack::Slice const&) const;

  /// @brief Type of this node (LEAF / NODE)
  NodeType type() const;

  /// @brief Get child specified by name
  Node& operator [](std::string name);
  Node const& operator [](std::string name) const;

  /// @brief Get node specified by path vector  
  Node& operator ()(std::vector<std::string>& pv);
  Node const& operator ()(std::vector<std::string>& pv) const;
  
  /// @brief Get node specified by path string  
  Node& operator ()(std::string const& path);
  Node const& operator ()(std::string const& path) const;

  /// @brief Remove node with absolute path
  bool remove (std::string const& path);

  /// @brief Remove child 
  bool removeChild (std::string const& key);

  /// @brief Remove this node
  bool remove();

  /// @brief Root node
  Node& root();

  /// @brief Dump to ostream
  friend std::ostream& operator<<(std::ostream& os, const Node& n) {
    Node const* par = n._parent;
    while (par != 0) {
      par = par->_parent;
      os << "  ";
    }
    os << n._name << " : ";
    if (n.type() == NODE) {
      os << std::endl;
      for (auto const& i : n._children)
        os << *(i.second);
    } else {
      os << ((n.slice().type() == ValueType::None) ? "NONE" : n.slice().toJson()) << std::endl;
    }
    return os;
  }

  /// #brief Get path of this node
  std::string path (); 
  
  /// @brief Apply single slice
  bool applies (arangodb::velocypack::Slice const&);

  /// @brief Create Builder representing this store
  void toBuilder (Builder&) const;

  /// @brief Create slice from value
  Slice slice() const;

protected:

  /// @brief Add time to live entry
  virtual bool addTimeToLive (long millis);
  
  Node* _parent;
  Children _children;
  TimeTable _time_table;
  Buffer<uint8_t> _value;
  std::chrono::system_clock::time_point _ttl;
  
  NodeType _type;
  std::string _name;
  
};


/// @brief Key value tree 
class Store : public Node, public arangodb::Thread {
  
public:

  /// @brief Construct with name
  Store (std::string const& name = "root");

  /// @brief Destruct
  virtual ~Store ();

  /// @brief Apply entry in query 
  std::vector<bool> apply (query_t const& query);

  /// @brief Read specified query from store
  query_t read (query_t const& query) const;
  
private:
  /// @brief Read individual entry specified in slice into builder
  bool read  (arangodb::velocypack::Slice const&,
              arangodb::velocypack::Builder&) const;

  /// @brief Check precondition
  bool check (arangodb::velocypack::Slice const&) const;

  /// @brief Clear entries, whose time to live has expired
  void clearTimeTable ();

  /// @brief Begin shutdown of thread
  void beginShutdown () override;

  /// @brief Run thread
  void run () override final;

  /// @brief Condition variable guarding removal of expired entries
  arangodb::basics::ConditionVariable _cv;

  /// @brief Read/Write mutex on database
  mutable arangodb::Mutex _storeLock;
  
};

}}

#endif
