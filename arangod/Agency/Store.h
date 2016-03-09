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

#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>
#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

enum NodeType {NODE, LEAF};

inline std::ostream& operator<< (std::ostream& os, std::vector<std::string> const& sv) {
  for (auto const& i : sv)
    os << i << " ";
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

  Node& operator= (arangodb::velocypack::Slice const& t);

  Node& operator= (Node const& node);
  
  NodeType type() const;

  Node& operator [](std::string name);

  bool append (std::string const name,
               std::shared_ptr<Node> const node = nullptr);

  Node& operator ()(std::vector<std::string>& pv);
  
  Node const& operator ()(std::vector<std::string>& pv) const;
  
  Node const& operator ()(std::string const& path) const;

  Node& operator ()(std::string const& path);

  Node const& read (std::string const& path) const;

  Node& write (std::string const& path);

  friend std::ostream& operator<<(std::ostream& os, const Node& n);

  std::vector<bool> apply (query_t const& query);

  query_t read (query_t const& query) const;

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

protected:
  Node* _parent;
  bool apply (arangodb::velocypack::Slice const&);

private:

  bool check (arangodb::velocypack::Slice const&);

  typedef Buffer<uint8_t> value_t;

  NodeType _type;
  std::string _name;
  value_t _value;
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
