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

#include "UrlHelper.h"

#include <sstream>
#include <utility>

using namespace arangodb;
using namespace arangodb::url;

namespace {
// used for URI-encoding
static char const* hexValuesLower = "0123456789abcdef";
};

std::ostream& Query::toStream(std::ostream& ostream) const {
  struct output {
    std::ostream& ostream;

    std::ostream& operator()(QueryString const& queryString) {
      return ostream << queryString.value();
    }
    std::ostream& operator()(QueryParameters const& queryParameters) {
      return ostream << queryParameters;
    }
  };
  return std::visit(output{ostream}, _content);
}

Query::Query(QueryString queryString) : _content(queryString) {}
Query::Query(QueryParameters queryParameters) : _content(queryParameters) {}

bool Query::empty() const noexcept {
  struct output {
    bool operator()(QueryString const& queryString) const {
      return queryString.value().empty();
    }
    bool operator()(QueryParameters const& queryParameters) const {
      return queryParameters.empty();
    }
  };
  return std::visit(output{}, _content);
}

std::ostream& QueryParameters::toStream(std::ostream& ostream) const {
  bool first = true;
  for (auto const& it : _pairs) {
    if (!first) {
      ostream << "&";
    }
    first = false;
    ostream << uriEncode(it.first) << "=" << uriEncode(it.second);
  }

  return ostream;
}

void QueryParameters::add(std::string const& key, std::string const& value) {
  _pairs.emplace_back(key, value);
}

bool QueryParameters::empty() const noexcept { return _pairs.empty(); }

Scheme::Scheme(std::string scheme) : _value(std::move(scheme)) {}

std::string const& Scheme::value() const noexcept { return _value; }

User::User(std::string user) : _value(std::move(user)) {}

std::string const& User::value() const noexcept { return _value; }

Password::Password(std::string password) : _value(std::move(password)) {}

std::string const& Password::value() const noexcept { return _value; }

Host::Host(std::string host) : _value(std::move(host)) {}

std::string const& Host::value() const noexcept { return _value; }

Port::Port(uint16_t port) : _value(port) {}

uint16_t const& Port::value() const noexcept { return _value; }

Authority::Authority(std::optional<UserInfo> userInfo, Host host, std::optional<Port> port)
    : _userInfo(std::move(userInfo)), _host(std::move(host)), _port(std::move(port)) {}
std::optional<UserInfo> const& Authority::userInfo() const noexcept {
  return _userInfo;
}

Host const& Authority::host() const noexcept { return _host; }

std::optional<Port> const& Authority::port() const noexcept { return _port; }

UserInfo::UserInfo(User user, Password password)
    : _user(std::move(user)), _password(std::move(password)) {}

UserInfo::UserInfo(User user)
    : _user(std::move(user)), _password(std::nullopt) {}

User const& UserInfo::user() const noexcept { return _user; }

std::optional<Password> const& UserInfo::password() const noexcept {
  return _password;
}

Path::Path(std::string path) : _value(std::move(path)) {}
std::string const& Path::value() const noexcept { return _value; }

QueryString::QueryString(std::string queryString)
    : _value(std::move(queryString)) {}

std::string const& QueryString::value() const noexcept { return _value; }

Fragment::Fragment(std::string fragment) : _value(std::move(fragment)) {}

std::string const& Fragment::value() const noexcept { return _value; }

std::string Url::toString() const {
  std::stringstream url;
  url << *this;
  return url.str();
}

Url::Url(Scheme scheme, std::optional<Authority> authority, Path path,
         std::optional<Query> query, std::optional<Fragment> fragment)
    : _scheme(std::move(scheme)),
      _authority(std::move(authority)),
      _path(std::move(path)),
      _query(std::move(query)),
      _fragment(std::move(fragment)) {}

Scheme const& Url::scheme() const noexcept { return _scheme; }

std::optional<Authority> const& Url::authority() const noexcept {
  return _authority;
}

Path const& Url::path() const noexcept { return _path; }

std::optional<Query> const& Url::query() const noexcept { return _query; }

std::optional<Fragment> const& Url::fragment() const noexcept {
  return _fragment;
}

Location::Location(Path path, std::optional<Query> query, std::optional<Fragment> fragment)
    : _path(std::move(path)), _query(std::move(query)), _fragment(std::move(fragment)) {}

std::string Location::toString() const {
  std::stringstream location;
  location << *this;
  return location.str();
}

Path const& Location::path() const noexcept { return _path; }

std::optional<Query> const& Location::query() const noexcept {
  return _query;
}

std::optional<Fragment> const& Location::fragment() const noexcept {
  return _fragment;
}

// unreserved are A-Z, a-z, 0-9 and - _ . ~
bool arangodb::url::isUnreserved(char c) {
  return (c >= '0' && c <= '9') || 
         (c >= 'a' && c <= 'z') || 
         (c >= 'A' && c <= 'Z') || 
         c == '-' || c == '_' || c == '.' || c == '~';
}

// reserved are:
// ! * ' ( ) ; : @ & = + $ , / ? % # [ ]
bool arangodb::url::isReserved(char c) {
  return c == '!' || c == '*' || c == '\'' || c == '(' || c == ')' ||
         c == ';' || c == ':' || c == '@' || c == '&' || c == '=' || c == '+' ||
         c == '$' || c == ',' || c == '/' || c == '?' || c == '%' || c == '#' ||
         c == '[' || c == ']';
}

std::string arangodb::url::uriEncode(std::string const& raw) {
  std::string encoded;

  for (auto const c : raw) {
    if (isUnreserved(c)) {
      // append character as is
      encoded.push_back(c);
    } else {
      // must hex-encode the character
      encoded.push_back('%');
      auto u = static_cast<unsigned char>(c);
      encoded.push_back(::hexValuesLower[u >> 4]);
      encoded.push_back(::hexValuesLower[u % 16]);
    }
  }

  return encoded;
}

std::ostream& arangodb::url::operator<<(std::ostream& ostream, Location const& location) {
  ostream << location.path().value();

  if (location.query()) {
    ostream << "?" << *location.query();
  }

  if (location.fragment()) {
    ostream << "#" << location.fragment()->value();
  }

  return ostream;
}

std::ostream& arangodb::url::operator<<(std::ostream& ostream, Url const& url) {
  ostream << url.scheme().value() << ":";

  if (url.authority()) {
    ostream << "//" << *url.authority();
  }

  ostream << Location{url.path(), url.query(), url.fragment()};

  return ostream;
}

std::ostream& arangodb::url::operator<<(std::ostream& ostream, Authority const& authority) {
  if (authority.userInfo()) {
    ostream << *authority.userInfo() << "@";
  }
  ostream << authority.host().value();
  if (authority.port()) {
    ostream << authority.port()->value();
  }
  return ostream;
}

std::ostream& arangodb::url::operator<<(std::ostream& ostream, UserInfo const& userInfo) {
  ostream << userInfo.user().value();
  if (userInfo.password()) {
    ostream << ":" << userInfo.password()->value();
  }
  return ostream;
}

std::ostream& arangodb::url::operator<<(std::ostream& ostream, Query const& query) {
  return query.toStream(ostream);
}

std::ostream& arangodb::url::operator<<(std::ostream& ostream,
                                        QueryParameters const& queryParameters) {
  return queryParameters.toStream(ostream);
}
