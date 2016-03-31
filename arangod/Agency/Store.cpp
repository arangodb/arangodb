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

#include "Store.h"
#include "Agent.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <Basics/ConditionLocker.h>

#include <iostream>

using namespace arangodb::consensus;

struct NotEmpty {
  bool operator()(const std::string& s) { return !s.empty(); }
};
struct Empty {
  bool operator()(const std::string& s) { return s.empty(); }
};

std::vector<std::string> split(const std::string& value, char separator) {
  std::vector<std::string> result;
  std::string::size_type p = (value.find(separator) == 0) ? 1:0;
  std::string::size_type q;
  while ((q = value.find(separator, p)) != std::string::npos) {
    result.emplace_back(value, p, q - p);
    p = q + 1;
  }
  result.emplace_back(value, p);
  result.erase(std::find_if(result.rbegin(), result.rend(),
                            NotEmpty()).base(), result.end());
  return result;
}

Node::Node (std::string const& name) : _parent(nullptr), _node_name(name) {
  _value.clear();
}
Node::Node (std::string const& name, Node* parent) :
  _parent(parent), _node_name(name) {
  _value.clear();
}

Node::~Node() {}

VPackSlice Node::slice() const {
  return (_value.size()==0) ?
    VPackSlice("\x00a",&Options::Defaults):VPackSlice(_value.data());
}

std::string const& Node::name() const {return _node_name;}

Node& Node::operator= (VPackSlice const& slice) { // Assign value (become leaf)
  _children.clear();
  _value.reset();
  _value.append(reinterpret_cast<char const*>(slice.begin()), slice.byteSize());
  return *this;
}

Node& Node::operator= (Node const& node) { // Assign node
  _node_name = node._node_name;
  _type = node._type;
  _value = node._value;
  _children = node._children;
  _ttl = node._ttl;
  return *this;
}

bool Node::operator== (VPackSlice const& rhs) const {
  return rhs.equals(slice());
}

bool Node::remove (std::string const& path) {
  std::vector<std::string> pv = split(path, '/');
  std::string key(pv.back());
  pv.pop_back();
  try {
    Node& parent = (*this)(pv);
    return parent.removeChild(key);
  } catch (StoreException const& e) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Failed to delete key " << key;
    LOG_TOPIC(DEBUG, Logger::AGENCY) << e.what();
    return false;
  }
}

bool Node::remove () {
  Node& parent = *_parent;
  return parent.removeChild(_node_name);
}

bool Node::removeChild (std::string const& key) {
  auto found = _children.find(key);
  if (found == _children.end())
    return false;
  else
    _children.erase(found);
  return true;
}

NodeType Node::type() const {return _children.size() ? NODE : LEAF;}

Node& Node::operator [](std::string name) {
  return *_children[name];
}

Node& Node::operator ()(std::vector<std::string>& pv) {
  if (pv.size()) {
    std::string const key = pv[0];
    if (_children.find(key) == _children.end()) {
      _children[key] = std::make_shared<Node>(pv[0], this);
    }
    pv.erase(pv.begin());
    return (*_children[key])(pv);
  } else {
    return *this;
  }
}

Node const& Node::operator ()(std::vector<std::string>& pv) const {
  if (pv.size()) {
    std::string const key = pv[0];
    pv.erase(pv.begin());
    if (_children.find(key) == _children.end()) {
      throw StoreException("Not found");
    }
    const Node& child = *_children.at(key);
    return child(pv);
  } else {
    return *this;
  }
}
  
Node const& Node::operator ()(std::string const& path) const {
  PathType pv = split(path,'/');
  return this->operator()(pv);
}

Node& Node::operator ()(std::string const& path) {
  PathType pv = split(path,'/');
  return this->operator()(pv);
}

Node& Node::root() {
  Node *par = _parent, *tmp = 0;
  while (par != 0) {
    tmp = par;
    par = par->_parent;
  }
  return *tmp;
}

ValueType Node::valueType() const {
  return slice().type();
}

bool Node::addTimeToLive (long millis) {
  root()._time_table[
    std::chrono::system_clock::now() + std::chrono::milliseconds(millis)] =
    _parent->_children[_node_name];
  return true;
}

bool Node::applies (VPackSlice const& slice) {

  if (slice.type() == ValueType::Object) {
    
    for (auto const& i : VPackObjectIterator(slice)) {

      std::string key = i.key.copyString();
      
      if (slice.hasKey("op")) {
        
        std::string oper = slice.get("op").copyString();
        VPackSlice const& self = this->slice();
        if (oper == "delete") {
          return _parent->removeChild(_node_name);
        } else if (oper == "set") { //
          if (!slice.hasKey("new")) {
            LOG_TOPIC(WARN, Logger::AGENCY) << "Operator set without new value";
            LOG_TOPIC(WARN, Logger::AGENCY) << slice.toJson();
            return false;
          }
          if (slice.hasKey("ttl")) {
            VPackSlice ttl_v = slice.get("ttl");
            if (ttl_v.isNumber()) {
              long ttl = 1000l * (
                (ttl_v.isDouble()) ?
                static_cast<long>(slice.get("ttl").getDouble()):
                slice.get("ttl").getInt());
                addTimeToLive (ttl);
            } else {
              LOG_TOPIC(WARN, Logger::AGENCY) <<
                "Non-number value assigned to ttl: " << ttl_v.toJson();
            }
          }
          *this = slice.get("new");
          return true;
        } else if (oper == "increment") { // Increment
          Builder tmp;
          tmp.openObject();
          try {
            tmp.add("tmp", Value(self.getInt()+1));
          } catch (std::exception const& e) {
            tmp.add("tmp",Value(1));
          }
          tmp.close();
          *this = tmp.slice().get("tmp");
          return true;
        } else if (oper == "decrement") { // Decrement
          Builder tmp;
          tmp.openObject();
          try {
            tmp.add("tmp", Value(self.getInt()-1));
          } catch (std::exception const& e) {
            tmp.add("tmp",Value(-1));
          }
          tmp.close();
          *this = tmp.slice().get("tmp");
          return true;
        } else if (oper == "push") { // Push
          if (!slice.hasKey("new")) {
            LOG_TOPIC(WARN, Logger::AGENCY)
              << "Operator push without new value: " << slice.toJson();
            return false;
          }
          Builder tmp;
          tmp.openArray();
          if (self.isArray()) {
            for (auto const& old : VPackArrayIterator(self))
              tmp.add(old);
          }
          tmp.add(slice.get("new"));
          tmp.close();
          *this = tmp.slice();
          return true;
        } else if (oper == "pop") { // Pop
          Builder tmp;
          tmp.openArray();
          if (self.isArray()) {
            VPackArrayIterator it(self);
            size_t j = it.size()-1;
            for (auto old : it) {
              tmp.add(old);
              if (--j==0)
                break;
            }
          }
          tmp.close();
          *this = tmp.slice();
          return true;
        } else if (oper == "prepend") { // Prepend
          if (!slice.hasKey("new")) {
            LOG_TOPIC(WARN, Logger::AGENCY)
              << "Operator prepend without new value: " << slice.toJson();
            return false;
          }
          Builder tmp;
          tmp.openArray();
          tmp.add(slice.get("new"));
          if (self.isArray()) {
          for (auto const& old : VPackArrayIterator(self))
            tmp.add(old);
          }
          tmp.close();
          *this = tmp.slice();
          return true;
        } else if (oper == "shift") { // Shift
          Builder tmp;
          tmp.openArray();
          if (self.isArray()) { // If a
            VPackArrayIterator it(self);
            bool first = true;
            for (auto old : it) {
              if (first) {
                first = false;
              } else {
                tmp.add(old);
              }
            }
          } 
          tmp.close();
          *this = tmp.slice();
          return true;
        } else {
          LOG_TOPIC(WARN, Logger::AGENCY) << "Unknown operation " << oper;
          return false;
        }
      } else if (slice.hasKey("new")) { // new without set
        *this = slice.get("new");
        return true;
      } else if (key.find('/')!=std::string::npos) {
        (*this)(key).applies(i.value);
      } else {
        auto found = _children.find(key);
        if (found == _children.end()) {
          _children[key] = std::make_shared<Node>(key, this);
        }
      _children[key]->applies(i.value);
      }
    }
  } else {
    *this = slice;
  }
  return true;
}

void Node::toBuilder (Builder& builder) const {
  
  try {
    if (type()==NODE) {
      VPackObjectBuilder guard(&builder);
      for (auto const& child : _children) {
        builder.add(VPackValue(child.first));
        child.second->toBuilder(builder);
      }
    } else {
      builder.add(slice());
    }
  } catch (std::exception const& e) {
    LOG(FATAL) << e.what();
  }
  
}

std::ostream& Node::print (std::ostream& o) const {
  Node const* par = _parent;
  while (par != 0) {
      par = par->_parent;
      o << "  ";
  }
  o << _node_name << " : ";
  if (type() == NODE) {
    o << std::endl;
    for (auto const& i : _children)
      o << *(i.second);
  } else {
    o << ((slice().type() == ValueType::None) ? "NONE" : slice().toJson()) << std::endl;
  }
  return o;
}

Store::Store (std::string const& name) : Node(name), Thread(name) {}

Store::~Store () {}

std::vector<bool> Store::apply (query_t const& query) {    
  std::vector<bool> applied;
  MUTEX_LOCKER(storeLocker, _storeLock);
  for (auto const& i : VPackArrayIterator(query->slice())) {
    switch (i.length()) {
    case 1:
      applied.push_back(applies(i[0])); break; // no precond
    case 2:
      if (check(i[1])) {
        applied.push_back(applies(i[0]));      // precondition
      } else {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Precondition failed!";
        applied.push_back(false);
      }
      break;
    default:                                   // wrong
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "We can only handle log entry with or without precondition!";
      applied.push_back(false);
      break;
    }
  }
  _cv.signal();                                // Wake up run

  return applied;
}

std::vector<bool> Store::apply( std::vector<VPackSlice> const& queries) {
  std::vector<bool> applied;
  MUTEX_LOCKER(storeLocker, _storeLock);
  for (auto const& i : queries) {
    applied.push_back(applies(i)); // no precond
  }
  return applied;
}

bool Store::check (VPackSlice const& slice) const {
  if (slice.type() != VPackValueType::Object) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Cannot check precondition: "
                                    << slice.toJson();
    return false;
  }
  for (auto const& precond : VPackObjectIterator(slice)) {
    std::string path = precond.key.copyString();
    bool found = false;
    Node node ("precond");
    
    try {
      node = (*this)(path);
      found = true;
    } catch (StoreException const&) {}
    
    if (precond.value.type() == VPackValueType::Object) { 
      for (auto const& op : VPackObjectIterator(precond.value)) {
        std::string const& oper = op.key.copyString();
        if (oper == "old") {                           // old
          return (node == op.value);
        } else if (oper == "isArray") {                // isArray
          if (op.value.type()!=VPackValueType::Bool) { 
            LOG (FATAL) << "Non boolsh expression for 'isArray' precondition";
            return false;
          }
          bool isArray =
            (node.type() == LEAF &&
             node.slice().type() == VPackValueType::Array);
          return op.value.getBool() ? isArray : !isArray;
        } else if (oper == "oldEmpty") {              // isEmpty
          if (op.value.type()!=VPackValueType::Bool) { 
            LOG (FATAL) << "Non boolsh expression for 'oldEmpty' precondition";
            return false;
          }
          return op.value.getBool() ? !found : found;
        }
      }
    } else {
      return node == precond.value;
    }
  }
  
  return true;
}

std::vector<bool> Store::read (query_t const& queries, query_t& result) const { // list of list of paths
  std::vector<bool> success;
  MUTEX_LOCKER(storeLocker, _storeLock);
  if (queries->slice().type() == VPackValueType::Array) {
    result->add(VPackValue(VPackValueType::Array)); // top node array
    for (auto const& query : VPackArrayIterator(queries->slice())) {
      success.push_back(read (query, *result));
    } 
    result->close();
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Read queries to stores must be arrays";
  }
  return success;
}

bool Store::read (VPackSlice const& query, Builder& ret) const {

  bool success = true;
  
  // Collect all paths
  std::list<std::string> query_strs;
  if (query.type() == VPackValueType::Array) {
    for (auto const& sub_query : VPackArrayIterator(query))
      query_strs.push_back(sub_query.copyString());
  } else if (query.type() == VPackValueType::String) {
    query_strs.push_back(query.copyString());
  } else {
    return false;
  }
  query_strs.sort();     // sort paths
  
  // Remove double ranges (inclusion / identity)
  for (auto i = query_strs.begin(), j = i; i != query_strs.end(); ++i) {
    if (i!=j && i->compare(0,j->size(),*j)==0) {
      *i="";
    } else {
      j = i;
    }
  }
  auto cut = std::remove_if(query_strs.begin(), query_strs.end(), Empty());
  query_strs.erase (cut,query_strs.end());
  
  // Create response tree 
  Node copy("copy");
  for (auto i = query_strs.begin(); i != query_strs.end(); ++i) {
    try {
      copy(*i) = (*this)(*i);
    } catch (StoreException const& e) {
      if (query.type() == VPackValueType::String)
        success = false;
    }
  }
  
  // Assemble builder from response tree
  if (query.type() == VPackValueType::String &&
      copy(*query_strs.begin()).type() == LEAF) {
    ret.add(copy(*query_strs.begin()).slice());
  } else {
    if (copy.type() == LEAF && copy.valueType() == VPackValueType::Null) {
      ret.add(VPackValue(VPackValueType::Object));
      ret.close();
    } else {
      copy.toBuilder(ret);
    }
  }
  
  return success;
  
}

void Store::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}

void Store::clearTimeTable () {
  for (auto it = _time_table.cbegin(); it != _time_table.cend() ;) {
    // Remove expired from front 
    if (it->first < std::chrono::system_clock::now()) {
      it->second->remove();
      _time_table.erase(it++);
      
    } else {
      break;
    }
  }
}

bool Store::start () {
  Thread::start();
  return true;
}

bool Store::start (Agent* agent) {
  _agent = agent;
  return start();
}

void Store::run() {
  CONDITION_LOCKER(guard, _cv);
  while (!this->isStopping()) { // Check timetable and remove overage entries
    _cv.wait(100000);           // better wait to next known time point
    clearTimeTable();
  }
}



