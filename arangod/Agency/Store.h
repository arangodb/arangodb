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
#include <memory>
#include <cstdint>

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

enum NodeType {NODE, LEAF};

inline std::ostream& operator<< (std::ostream& os, std::vector<std::string> const& sv) {
  for (auto const& i : sv)
    os << i << " ";
  os << std::endl;
  return os;
}


using namespace arangodb::velocypack;

enum NODE_EXPECTION {PATH_NOT_FOUND};

class Node {
public:

  typedef std::vector<std::string> PathType;
  typedef std::map<std::string, std::shared_ptr<Node>> Children;
  
  Node (std::string const& name);
  
  ~Node ();
  
  std::string const& name() const;

  template<class T> Node& operator= (T const& t) { // Assign value (become leaf)
    _children.clear();
    _value = t;
    return *this;
  }

  Node& operator= (Node const& node) { // Assign node
    *this = node;
    return *this;
  }

  inline NodeType type() const {return _children.size() ? NODE : LEAF;}

  Node& operator [](std::string name) {
    return *_children[name];
  }

  bool append (std::string const name, std::shared_ptr<Node> const node = nullptr) {
    if (node != nullptr) {
      _children[name] = node;
    } else {
      _children[name] = std::make_shared<Node>(name);
    }
    _children[name]->_parent = this;
    return true;
  }

  Node& operator ()(std::vector<std::string>& pv) {
    switch (pv.size()) {
    case 0: // path empty
      return *this;
      break;
    default: // at least one depth left
      auto found = _children.find(pv[0]);
      if (found == _children.end()) {
        _children[pv[0]] = std::make_shared<Node>(pv[0]);
        found = _children.find(pv[0]);
        found->second->_parent = this;
      }
      pv.erase(pv.begin());
      return (*found->second)(pv);
      break;
    }
  }
  
  Node const& operator ()(std::vector<std::string>& pv) const {
    switch (pv.size()) {
    case 0: // path empty
      return *this;
      break;
    default: // at least one depth left
      auto found = _children.find(pv[0]);
      if (found == _children.end()) {
        throw PATH_NOT_FOUND;
      }
      pv.erase(pv.begin());
      return (*found->second)(pv);
      break;
    }
  }
  
  Node const& operator ()(std::string const& path) const {
    PathType pv = split(path,"/");
    return this->operator()(pv);
  }

  Node& operator ()(std::string const& path) {
    PathType pv = split(path,"/");
    return this->operator()(pv);
  }

  Node const& read (std::string const& path) const {
    PathType pv = split(path,"/");
    return this->operator()(pv);
  }

  Node& write (std::string const& path) {
    PathType pv = split(path,"/");
    return this->operator()(pv);
  }

  friend std::ostream& operator<<(std::ostream& os, const Node& n) {
    Node* par = n._parent;
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
      os << n._value.toString() << std::endl;
    }
    return os;
  }

  std::vector<bool> apply (query_t const& query) {    
    std::vector<bool> applied;
    MUTEX_LOCKER(mutexLocker, _storeLock);
    for (auto const& i : VPackArrayIterator(query->slice())) {
      applied.push_back(apply(i));
    }
    return applied;
  }

  query_t read (query_t const& query) const {
    MUTEX_LOCKER(mutexLocker, _storeLock);
    // TODO: Run through JSON and asseble result
    query_t ret = std::make_shared<arangodb::velocypack::Builder>();
    return ret;
  }

protected:
  Node* _parent;
private:
  bool apply (arangodb::velocypack::Slice const& slice) {
    // TODO apply slice to database
    return true;
  }
  
  NodeType _type;
  std::string _name;
  Buffer<uint8_t> _value;
  Children _children;
  mutable arangodb::Mutex _storeLock;
  
};


class Store : public Node { // Root node
  
public:
  Store () : Node("root") {}
  ~Store () {} 

};

}}

#endif
