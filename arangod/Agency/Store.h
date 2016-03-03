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

#include <type_traits>
#include <utility>
#include <typeinfo>
#include <string>
#include <cassert>
#include <map>
#include <vector>
#include <memory>
#include <regex>

namespace arangodb {
namespace consensus {

template<class T> struct TypeTraits {
  static bool supported () {return false;}
};
template<> struct TypeTraits<int64_t> {
  static bool supported () {return false;}
};
template<> struct TypeTraits<uint64_t> {
  static bool supported () {return false;}
};
template<> struct TypeTraits<double> {
  static bool supported () {return false;}
};
template<> struct TypeTraits<std::string> {
  static bool supported () {return false;}
};

template<class T>
using Type = typename std::decay<typename std::remove_reference<T>::type>::type;

struct Any {

  bool is_null() const { return !_base_ptr; }
  bool not_null() const { return _base_ptr; }

  template<typename S> Any(S&& value)
    : _base_ptr(new Derived<Type<S>>(std::forward<S>(value))) {}

  template<class S> bool is() const {
    typedef Type<S> T;
    auto derived = dynamic_cast<Derived<T>*> (_base_ptr);
    return derived;
  }

  template<class S> Type<S>& as() const {
    typedef Type<S> T;
    auto derived = dynamic_cast<Derived<T>*> (_base_ptr);
    if (!derived)
      throw std::bad_cast();
    return derived->value;
  }

  template<class S> operator S() {
    return as<Type<S>>();
  }

  Any() : _base_ptr(nullptr) {}
  
  Any(Any& that) : _base_ptr(that.clone()) {}

  Any(Any&& that) : _base_ptr(that._base_ptr) {
    that._base_ptr = nullptr;
  }

  Any(const Any& that) : _base_ptr(that.clone()) {}

  Any(const Any&& that) : _base_ptr(that.clone()) {}

  Any& operator=(const Any& a) {
    if (_base_ptr == a._base_ptr)
      return *this;
    auto old__base_ptr = _base_ptr;
    _base_ptr = a.clone();
    if (old__base_ptr)
      delete old__base_ptr;
    return *this;
  }

  Any& operator=(Any&& a) {
    if (_base_ptr == a._base_ptr)
      return *this;
    std::swap(_base_ptr, a._base_ptr);
    return *this;
  }

  ~Any() {
    if (_base_ptr)
      delete _base_ptr;
  }

  friend std::ostream& operator<<(std::ostream& os, const Any& a) {
    try {
      os << a.as<double>();
    } catch (std::bad_cast const&) {
      try {
        os << a.as<int>();
      } catch (std::bad_cast const&) {
        try {
          os << "\"" << a.as<char const*>() << "\"";
        } catch (std::bad_cast const& e) {
          throw e;
        }
      }
    }
    return os;
  }
  
private:

  struct Base {
    virtual ~Base() {}
    virtual Base* clone() const = 0;    
  };
  
  template<typename T> struct Derived : Base {
    template<typename S> Derived(S&& value) : value(std::forward<S>(value)) { }
    T value;
    Base* clone() const { return new Derived<T>(value); }
  };
  
  Base* clone() const {
    if (_base_ptr)
      return _base_ptr->clone();
    else
      return nullptr;
  }
  
  Base* _base_ptr;

};

static inline std::vector<std::string>
split (std::string str, const std::string& dlm) {
	std::vector<std::string> sv;
  std::regex slashes("/+");
  str = std::regex_replace (str, slashes, "/");
	size_t  start = (str.find('/') == 0) ? 1:0, end = 0;
	while (end != std::string::npos) {
		end = str.find (dlm, start);
		sv.push_back(str.substr(start, (end == std::string::npos) ? std::string::npos : end - start));
		start = ((end > (std::string::npos - dlm.size())) ? std::string::npos : end + dlm.size());
	}
	return sv;
}

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

  Node (std::string const& name) : _name(name), _parent(nullptr), _value("") {}

  ~Node () {}

  std::string const& name() const {return _name;}

  template<class T>
  Node& operator= (T const& t) { // Assign value (become leaf)
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
      os << n._value << std::endl;
    }
    return os;
  }

protected:
  Node* _parent;
private:
  NodeType _type;
  std::string _name;
  Any _value;
  Children _children;
};


class Store : public Node { // Root node
  
public:
  Store () : Node("root") {}
  ~Store () {} 

};

}}

#endif
