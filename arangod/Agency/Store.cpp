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

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/Hexdump.h>

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

Node::Node (std::string const& name) : _parent(nullptr), _name(name) {
  _value.clear();
}
Node::Node (std::string const& name, Node* parent) :
  _parent(parent), _name(name) {
  _value.clear();
}

Node::~Node() {}

Slice Node::slice() const {
  return (_value.size()==0) ?
    Slice("\x018",&Options::Defaults):Slice(_value.data());
}

std::string const& Node::name() const {return _name;}

Node& Node::operator= (Slice const& slice) { // Assign value (become leaf)
  _children.clear();
  _value.reset();
  _value.append(reinterpret_cast<char const*>(slice.begin()), slice.byteSize());
  return *this;
}

Node& Node::operator= (Node const& node) { // Assign node
  _name = node._name;
  _type = node._type;
  _value = node._value;
  _children = node._children;
  _ttl = node._ttl;
  return *this;
}

bool Node::operator== (arangodb::velocypack::Slice const& rhs) const {
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
    return false;
  }
}

bool Node::remove () {
  Node& parent = *_parent;
  return parent.removeChild(_name);
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

std::ostream& operator<< (
  std::ostream& o, std::chrono::system_clock::time_point const& t) {
  std::time_t tmp = std::chrono::system_clock::to_time_t(t);
  o << std::ctime(&tmp);
  return o;
}
template<class S, class T>
std::ostream& operator<< (std::ostream& o, std::map<S,T> const& d) {
  for (auto const& i : d)
    o << i.first << ":" << i.second << std::endl;
  return o;
}

Node& Node::root() {
  Node *par = _parent, *tmp;
  while (par != 0) {
    tmp = par;
    par = par->_parent;
  }
  std::cout << par << std::endl;
  return *tmp;
}

bool Node::addTimeToLive (long millis) {
  root()._time_table[
    std::chrono::system_clock::now() + std::chrono::milliseconds(millis)] =
    _parent->_children[_name];
  return true;
}

bool Node::applies (arangodb::velocypack::Slice const& slice) {

  if (slice.type() == ValueType::Object) {

    for (auto const& i : VPackObjectIterator(slice)) {
      std::string key = i.key.toString();
      key = key.substr(1,key.length()-2);

      if (slice.hasKey("op")) {
        std::string oper = slice.get("op").toString();
        oper = oper.substr(1,oper.length()-2);
        Slice const& self = this->slice();
        if (oper == "delete") {
          return _parent->removeChild(_name);
        } else if (oper == "set") { //
          if (!slice.hasKey("new")) {
            LOG(WARN) << "Operator set without new value";
            LOG(WARN) << slice.toJson();
            return false;
          }
          if (slice.hasKey("ttl")) {
            addTimeToLive ((long)slice.get("ttl").getDouble()*1000);
          }
          *this = slice.get("new");
          return true;
        } else if (oper == "increment") { // Increment
          if (!(self.isInt() || self.isUInt())) {
            LOG(WARN) << "Element to increment must be integral type";
            LOG(WARN) << slice.toJson();
            return false;
          }
          Builder tmp;
          tmp.add(Value(self.isInt() ? int64_t(self.getInt()+1) :
                        uint64_t(self.getUInt()+1)));
          *this = tmp.slice();
          return true;
        } else if (oper == "decrement") { // Decrement
          if (!(self.isInt() || self.isUInt())) {
            LOG(WARN) << "Element to decrement must be integral type";
            LOG(WARN) << slice.toJson();
            return false;
          }
          Builder tmp;
          tmp.add(Value(self.isInt() ? int64_t(self.getInt()-1) :
                        uint64_t(self.getUInt()-1)));
          *this = tmp.slice();
          return true;
        } else if (oper == "push") { // Push
          if (!self.isArray()) {
            LOG(WARN) << "Push operation only on array types! We are " <<
              self.toString();
          }
          if (!slice.hasKey("new")) {
            LOG(WARN) << "Operator push without new value";
            LOG(WARN) << slice.toJson();
            return false;
          }
          Builder tmp;
          tmp.openArray();
          for (auto const& old : VPackArrayIterator(self))
            tmp.add(old);
          tmp.add(slice.get("new"));
          tmp.close();
          *this = tmp.slice();
          return true;
        } else if (oper == "pop") { // Pop
          if (!self.isArray()) {
            LOG(WARN) << "Pop operation only on array types! We are " <<
              self.toString();
          }
          Builder tmp;
          tmp.openArray();
          VPackArrayIterator it(self);
          size_t j = it.size()-1;
          for (auto old : it) {
            tmp.add(old);
            if (--j==0)
              break;
          }
          tmp.close();
          *this = tmp.slice();
          return true;
        } else if (oper == "prepend") { // Prepend
          if (!self.isArray()) {
            LOG(WARN) << "Prepend operation only on array types! We are " <<
              self.toString();
          }
          if (!slice.hasKey("new")) {
            LOG(WARN) << "Operator push without new value";
            LOG(WARN) << slice.toJson();
            return false;
          }
          Builder tmp;
          tmp.openArray();
          tmp.add(slice.get("new"));
          for (auto const& old : VPackArrayIterator(self))
            tmp.add(old);
          tmp.close();
          *this = tmp.slice();
          return true;
        } else if (oper == "shift") { // Shift
          if (!self.isArray()) {
            LOG(WARN) << "Shift operation only on array types! We are " <<
              self.toString();
          }
          Builder tmp;
          tmp.openArray();
          VPackArrayIterator it(self);
          bool first = true;
          for (auto old : it) {
            if (first) {
              first = false;
            } else {
              tmp.add(old);
            }
          }
          tmp.close();
          *this = tmp.slice();
          return true;
        } else {
          LOG(WARN) << "Unknown operation " << oper;
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
    std::cout << e.what() << std::endl;
  }
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
        LOG(WARN) << "Precondition failed!";
        applied.push_back(false);
      }
      break;
    default:                                   // wrong
      LOG(FATAL) << "We can only handle log entry with or without precondition!";
      applied.push_back(false);
      break;
    }
  }
  _cv.signal();                                // Wake up run

  return applied;
}

bool Store::check (arangodb::velocypack::Slice const& slice) const {
  if (slice.type() != VPackValueType::Object) {
    LOG(WARN) << "Cannot check precondition: " << slice.toJson();
    return false;
  }
  for (auto const& precond : VPackObjectIterator(slice)) {
    std::string path = precond.key.toString();
    path = path.substr(1,path.size()-2);

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

query_t Store::read (query_t const& queries) const { // list of list of paths
  MUTEX_LOCKER(storeLocker, _storeLock);
  query_t result = std::make_shared<arangodb::velocypack::Builder>();
  if (queries->slice().type() == VPackValueType::Array) {
    result->add(VPackValue(VPackValueType::Array)); // top node array
    for (auto const& query : VPackArrayIterator(queries->slice())) {
      read (query, *result);
    } 
    result->close();
  } else {
    LOG(FATAL) << "Read queries to stores must be arrays";
  }
  return result;
}

bool Store::read (arangodb::velocypack::Slice const& query, Builder& ret) const {

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
    } catch (StoreException const&) {}
  }
  // Assemble builder from response tree
  if (query_strs.size() == 1 && copy(*query_strs.begin()).type() == LEAF) {
    ret.add(copy(*query_strs.begin()).slice());
  } else {
    copy.toBuilder(ret);
  }
  
  return true;
}

void Store::beginShutdown() {
  Thread::beginShutdown();
}

void Store::clearTimeTable () {
  for (auto it = _time_table.cbegin(); it != _time_table.cend() ;) {
    if (it->first < std::chrono::system_clock::now()) {
      it->second->remove();
      _time_table.erase(it++);
    } else {
      break;
    }
  }
}

void Store::run() {
  CONDITION_LOCKER(guard, _cv);
  
  while (!this->isStopping()) { // Check timetable and remove overage entries
    _cv.wait(200000); // better wait to next known time point
    clearTimeTable();
  }
}


