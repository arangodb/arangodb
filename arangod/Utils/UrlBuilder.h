////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_URLBUILDER_H
#define ARANGOD_UTILS_URLBUILDER_H

#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arangodb {
namespace url {

// TODO Add string validation

class Scheme {
 public:
  explicit Scheme(std::string);
  std::string const& value() const noexcept;

 private:
  std::string _value;
};

class User {
 public:
  explicit User(std::string);
  std::string const& value() const noexcept;

 private:
  std::string _value;
};

class Password {
 public:
  explicit Password(std::string);
  std::string const& value() const noexcept;

 private:
  std::string _value;
};

class UserInfo {
 public:
  UserInfo(User, Password);
  explicit UserInfo(User);

  User const& user() const noexcept;
  boost::optional<Password> const& password() const noexcept;

 private:
  User _user;
  boost::optional<Password> _password;
};

class Host {
 public:
  explicit Host(std::string);
  std::string const& value() const noexcept;

 private:
  std::string _value;
};

class Port {
 public:
  explicit Port(uint16_t);
  uint16_t const& value() const noexcept;

 private:
  uint16_t _value;
};

class Authority {
 public:
  Authority(boost::optional<UserInfo> userInfo, Host host, boost::optional<Port> port);

  boost::optional<UserInfo> const& userInfo() const noexcept;
  Host const& host() const noexcept;
  boost::optional<Port> const& port() const noexcept;

 private:
  boost::optional<UserInfo> _userInfo;
  Host _host;
  boost::optional<Port> _port;
};

class Path {
 public:
  explicit Path(std::string);
  std::string const& value() const noexcept;

 private:
  std::string _value;
};

class QueryString {
 public:
  explicit QueryString(std::string);
  std::string const& value() const noexcept;

 private:
  std::string _value;
};

// TODO Add a QueryParameterMap as an option?
//      This should maybe support arrays (e.g. foo[]=bar)?
class QueryParameters {
 public:
  // Keys and values will be url-encoded as necessary
  void add(std::string const& key, std::string const& value);

  bool empty() const noexcept;

  std::ostream& toStream(std::ostream&) const;

 private:
  std::vector<std::pair<std::string, std::string>> _pairs;
};

class Query {
 public:
  explicit Query(QueryString);
  explicit Query(QueryParameters);

  bool empty() const noexcept;

  std::ostream& toStream(std::ostream&) const;

 private:
  // Optionally use either a string, or a vector of pairs as representation
  boost::variant<QueryString, QueryParameters> _content;
};
struct Fragment {
  std::string value;
};

// This mostly adheres to the URL specification. However, Scheme is optional
// here, while for URLs it is mandatory, so we can build a path plus optional
// query string.
class Url {
 public:
  Url(Scheme, Path);
  explicit Url(Path);

  std::string toString() const;

  // TODO Remove this, rather build Url monolithically.
  //  For this, maybe move the "optionality" to the member types. E.g.,
  //  don't use boost::optional<Authority> but Authority, and make this
  //  optionally hold a value.
  void setQueryUnlessEmpty(Query const& query);

  boost::optional<Scheme> const& scheme() const noexcept;
  boost::optional<Authority> const& authority() const noexcept;
  Path const& path() const noexcept;
  boost::optional<Query> const& query() const noexcept;
  boost::optional<Fragment> const& fragment() const noexcept;

 private:
  boost::optional<Scheme> _scheme;
  boost::optional<Authority> _authority;
  Path _path;
  boost::optional<Query> _query;
  boost::optional<Fragment> _fragment;
};

std::string uriEncode(std::string const&);
bool isUnreserved(char);
bool isReserved(char);

std::ostream& operator<<(std::ostream&, Authority const&);
std::ostream& operator<<(std::ostream&, Query const&);
std::ostream& operator<<(std::ostream&, QueryParameters const&);
std::ostream& operator<<(std::ostream&, Url const&);
std::ostream& operator<<(std::ostream&, UserInfo const&);

}  // namespace url
}  // namespace arangodb

#endif  // ARANGOD_UTILS_URLBUILDER_H
