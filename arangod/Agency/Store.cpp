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


using namespace arangodb::consensus;


static inline std::vector<std::string> split (
  std::string str, const std::string& dlm) {
	std::vector<std::string> sv;
	size_t  start = (str.find('/') == 0) ? 1:0, end = 0;
	while (end != std::string::npos) {
		end = str.find (dlm, start);
		sv.push_back(str.substr(start, (end == std::string::npos) ?
                            std::string::npos : end - start));
		start = ((end > (std::string::npos - dlm.size())) ?
             std::string::npos : end + dlm.size());
	}
	return sv;
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

inline NodeType Node::type() const {return _children.size() ? NODE : LEAF;}

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
#include <iostream>

Node& Node::operator ()(std::vector<std::string>& pv) {
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
  PathType pv = split(path,"/");
  return this->operator()(pv);
}

Node& Node::operator ()(std::string const& path) {
  PathType pv = split(path,"/");
  return this->operator()(pv);
}

Node const& Node::read (std::string const& path) const {
  PathType pv = split(path,"/");
  return this->operator()(pv);
}

Node& Node::write (std::string const& path) {
  PathType pv = split(path,"/");
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

bool Node::check (arangodb::velocypack::Slice const& slice) {
  return true;
}

query_t Node::read (query_t const& query) const {
  MUTEX_LOCKER(storeLocker, _storeLock);
  // TODO: Run through JSON and asseble result
  query_t ret = std::make_shared<arangodb::velocypack::Builder>();
  return ret;
}


