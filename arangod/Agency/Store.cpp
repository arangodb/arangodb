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

#include <iostream>

using namespace arangodb::consensus;

struct NotEmpty {
  bool operator()(const std::string& s) { return !s.empty(); }
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

Node::Node (std::string const& name) : _parent(nullptr), _name(name) {}

Node::~Node() {}

std::string const& Node::name() const {return _name;}

Node& Node::operator= (Slice const& slice) { // Assign value (become leaf)
  _children.clear();
  _value.reset();
  _value.append((char const*)slice.begin(), slice.byteSize());
  return *this;
}

Node& Node::operator= (Node const& node) { // Assign node
  *this = node;
  return *this;
}

NodeType Node::type() const {return _children.size() ? NODE : LEAF;}

Node& Node::operator [](std::string name) {
  return *_children[name];
}

bool Node::append (std::string const name, std::shared_ptr<Node> const node) {
  if (node != nullptr) {
    _children[name] = node;
  } else {
    _children[name] = std::make_shared<Node>(name);
  }
  _children[name]->_parent = this;
  return true;
}

Node& Node::operator ()(std::vector<std::string>& pv) {
  std::cout << "const" << pv << pv.size() << std::endl;
  if (pv.size()) {
    auto found = _children.find(pv[0]);
    if (found == _children.end()) {
      _children[pv[0]] = std::make_shared<Node>(pv[0]);
      found = _children.find(pv[0]);
      found->second->_parent = this;
    }
    pv.erase(pv.begin());
    return (*found->second)(pv);
  } else {
    return *this;
  }
}

Node const& Node::operator ()(std::vector<std::string>& pv) const {
  std::cout << "non const" << std::endl;
  if (pv.size()) {
    auto found = _children.find(pv[0]);
    if (found == _children.end()) {
      throw PATH_NOT_FOUND;
    }
    pv.erase(pv.begin());
    return (*found->second)(pv);
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

Node const& Node::read (std::string const& path) const {
  PathType pv = split(path,'/');
  return this->operator()(pv);
}

Node& Node::write (std::string const& path) {
  PathType pv = split(path,'/');
  return this->operator()(pv);
}

std::vector<bool> Node::apply (query_t const& query) {    
  std::vector<bool> applied;
  std::vector<std::string> path;
  MUTEX_LOCKER(storeLocker, _storeLock);
  for (auto const& i : VPackArrayIterator(query->slice())) {
    switch (i.length()) {
    case 1:
      applied.push_back(apply(i[0])); break; // no precond
    case 2:
      if (check(i[0])) {
        applied.push_back(apply(i[1]));      // precondition
      } else {
        LOG(WARN) << "Precondition failed!";
        applied.push_back(false);
      }
      break;
    default:                                 // wrong
      LOG(FATAL) << "We can only handle log entry with or without precondition!";
      applied.push_back(false);
      break;
    }
  }
  return applied;
}

bool Node::apply (arangodb::velocypack::Slice const& slice) {
  if (slice.type() == ValueType::Object) {
    for (auto const& i : VPackObjectIterator(slice)) {
      std::string key = i.key.toString();
      key = key.substr(1,key.length()-2);
      auto found = _children.find(key);
      if (found == _children.end()) {
        _children[key] = std::make_shared<Node>(key);
        found = _children.find(key);
        found->second->_parent = this;
      }
      found->second->apply(i.value);
    }
  } else {
    *this = slice;
  }
  return true;
}

bool Node::check (arangodb::velocypack::Slice const& slice) const{
  return true;
}

query_t Node::read (query_t const& query) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  query_t result = std::make_shared<arangodb::velocypack::Builder>();
  result->add(VPackValue(VPackValueType::Object));
  for (auto const& i : VPackArrayIterator(query->slice())) {
    read (i, *result);
  }  
  // TODO: Run through JSON and asseble result
  result->close();
  return result;
}

bool Node::read (arangodb::velocypack::Slice const& slice,
                 Builder& ret) const {
  LOG(WARN)<<slice;
  if (slice.type() == ValueType::Object) {
    for (auto const& i : VPackObjectIterator(slice)) {
      std::string key = i.key.toString();
      auto found = _children.find(key);
      if (found == _children.end()) {
        return false;
      }
      found->second->read(i.value, ret);
    }
  } else {
    ret.add(slice);
  }
  return true;
}


