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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_URLHELPER_H
#define ARANGOD_UTILS_URLHELPER_H

#include <optional>
#include <variant>

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
  std::optional<Password> const& password() const noexcept;

 private:
  User _user;
  std::optional<Password> _password;
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
  Authority(std::optional<UserInfo> userInfo, Host host, std::optional<Port> port);

  std::optional<UserInfo> const& userInfo() const noexcept;
  Host const& host() const noexcept;
  std::optional<Port> const& port() const noexcept;

 private:
  std::optional<UserInfo> _userInfo;
  Host _host;
  std::optional<Port> _port;
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
  std::variant<QueryString, QueryParameters> _content;
};

class Fragment {
 public:
  explicit Fragment(std::string);

  std::string const& value() const noexcept;

 private:
  std::string _value;
};

class Url {
 public:
  Url(Scheme, std::optional<Authority>, Path, std::optional<Query>,
      std::optional<Fragment>);

  std::string toString() const;

  Scheme const& scheme() const noexcept;
  std::optional<Authority> const& authority() const noexcept;
  Path const& path() const noexcept;
  std::optional<Query> const& query() const noexcept;
  std::optional<Fragment> const& fragment() const noexcept;

 private:
  Scheme _scheme;
  std::optional<Authority> _authority;
  Path _path;
  std::optional<Query> _query;
  std::optional<Fragment> _fragment;
};

// Artificial part of an URI, including path and optionally query and fragment,
// but omitting scheme and authority.
class Location {
 public:
  Location(Path, std::optional<Query>, std::optional<Fragment>);

  std::string toString() const;

  Path const& path() const noexcept;
  std::optional<Query> const& query() const noexcept;
  std::optional<Fragment> const& fragment() const noexcept;

 private:
  Path _path;
  std::optional<Query> _query;
  std::optional<Fragment> _fragment;
};

std::string uriEncode(std::string const&);
bool isUnreserved(char);
bool isReserved(char);

std::ostream& operator<<(std::ostream&, Authority const&);
std::ostream& operator<<(std::ostream&, Query const&);
std::ostream& operator<<(std::ostream&, QueryParameters const&);
std::ostream& operator<<(std::ostream&, Location const&);
std::ostream& operator<<(std::ostream&, Url const&);
std::ostream& operator<<(std::ostream&, UserInfo const&);

}  // namespace url
}  // namespace arangodb

#endif  // ARANGOD_UTILS_URLHELPER_H
