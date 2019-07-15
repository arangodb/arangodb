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

#include "UrlBuilder.h"

#include <cctype>
#include <iomanip>
#include <sstream>

using namespace arangodb;
using namespace arangodb::url;

std::ostream& operator<<(std::ostream& ostream, Url const& url) {
  if (url.scheme()) {
    ostream << url.scheme()->value() << ":";
  }

  if (url.authority()) {
    ostream << "//" << *url.authority();
  }

  ostream << url.path().value;

  if (url.query()) {
    ostream << "?" << *url.query();
  }

  if (url.fragment()) {
    ostream << "#" << url.fragment()->value;
  }

  return ostream;
}

std::string Url::toString() const {
  std::stringstream url;
  url << *this;
  return url.str();
}

Url::Url(Scheme scheme, Path path)
    : _scheme(std::move(scheme)), _path(std::move(path)) {}

Url::Url(Path path) : _path(std::move(path)) {}

void Url::setQueryUnlessEmpty(const Query& query) {
  if (!query.empty()) {
    _query = query;
  }
}

boost::optional<Scheme> const& Url::scheme() const noexcept { return _scheme; }

boost::optional<Authority> const& Url::authority() const noexcept {
  return _authority;
}

Path const& Url::path() const noexcept { return _path; }

boost::optional<Query> const& Url::query() const noexcept { return _query; }

boost::optional<Fragment> const& Url::fragment() const noexcept {
  return _fragment;
}

std::ostream& operator<<(std::ostream& ostream, Authority const& authority) {
  if (authority.userInfo) {
    ostream << *authority.userInfo << "@";
  }
  ostream << authority.host.value();
  if (authority.port) {
    ostream << authority.port->value();
  }
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, UserInfo const& userInfo) {
  ostream << userInfo.user().value();
  if (userInfo.password()) {
    ostream << ":" << userInfo.password()->value();
  }
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, Query const& query) {
  return query.toStream(ostream);
}

std::ostream& Query::toStream(std::ostream& ostream) const {
  struct output {
    std::ostream& ostream;

    std::ostream& operator()(QueryString const& queryString) {
      return ostream << queryString.value;
    }
    std::ostream& operator()(QueryParameters const& queryParameters) {
      return ostream << queryParameters;
    }
  };
  return boost::apply_visitor(output{ostream}, _content);
}

Query::Query(QueryString queryString) : _content(queryString) {}
Query::Query(QueryParameters queryParameters) : _content(queryParameters) {}

bool Query::empty() const noexcept {
  struct output {
    bool operator()(QueryString const& queryString) {
      return queryString.value.empty();
    }
    bool operator()(QueryParameters const& queryParameters) {
      return queryParameters.empty();
    }
  };
  return boost::apply_visitor(output{}, _content);
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

std::ostream& operator<<(std::ostream& ostream, QueryParameters const& queryParameters) {
  return queryParameters.toStream(ostream);
}

void QueryParameters::add(std::string const& key, std::string const& value) {
  _pairs.emplace_back(key, value);
}

bool QueryParameters::empty() const noexcept { return _pairs.empty(); }

std::string uriEncode(std::string const& raw) {
  std::stringstream encoded;

  encoded << std::hex << std::setfill('0');

  for (auto const c : raw) {
    if (isUnreserved(c)) {
      encoded << c;
    } else {
      encoded << '%' << std::setw(2) << static_cast<unsigned>(c);
    }
  }

  return encoded.str();
}

// unreserved are A-Z, a-z, 0-9 and - _ . ~
bool isUnreserved(char c) {
  return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

// reserved are:
// ! * ' ( ) ; : @ & = + $ , / ? % # [ ]
bool isReserved(char c) {
  return c == '!' || c == '*' || c == '\'' || c == '(' || c == ')' ||
         c == ';' || c == ':' || c == '@' || c == '&' || c == '=' || c == '+' ||
         c == '$' || c == ',' || c == '/' || c == '?' || c == '%' || c == '#' ||
         c == '[' || c == ']';
}

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

UserInfo::UserInfo(User user, Password password)
    : _user(std::move(user)), _password(std::move(password)) {}

UserInfo::UserInfo(User user)
    : _user(std::move(user)), _password(boost::none) {}

User const& UserInfo::user() const noexcept { return _user; }

boost::optional<Password> const& UserInfo::password() const noexcept {
  return _password;
}
