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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "LdapUrlParser.h"

#include <velocypack/StringRef.h>

#include <regex>

using namespace arangodb;

std::string LdapUrlParseResult::toString() const {
  std::string result("ldap://");
  if (protocol.set) {
    result = protocol.value + "://";
  }
  if (host.set) {
    result.append(host.value);
    if (port.set) {
      result.push_back(':');
      result.append(port.value);
    }
  }
  result.push_back('/');
  result.append(basedn.value);
  if (searchAttribute.set) {
    result.push_back('?');
    result.append(searchAttribute.value);
  }
  if (deep.set) {
    result.push_back('?');
    result.append(deep.value);
  }
  return result;
}

LdapUrlParseResult LdapUrlParser::parse(std::string const& url) {
  LdapUrlParseResult result;
  parse(url, result);
  return result;
}

void LdapUrlParser::parse(std::string const& url, LdapUrlParseResult& result) {
  result.valid = true;

  arangodb::velocypack::StringRef view(url);
  if (view.substr(0, 7).compare("ldap://") == 0) {
    // skip over initial "ldap://"
    view = arangodb::velocypack::StringRef(view.data() + 7);
    result.protocol.populate(std::string("ldap"));
  } else if (view.substr(0, 8).compare("ldaps://") == 0) {
    view = arangodb::velocypack::StringRef(view.data() + 8);
    result.protocol.populate(std::string("ldaps"));
  } else {
    result.protocol.populate(std::string("ldap"));
  }

  if (!view.empty() && view[0] != '/') {
    // we have a hostname (and probably port)
    // now search for the '/' or '?' that marks the end of the host name
    size_t l = view.size();
    size_t pos = view.find('/');
    if (pos != std::string::npos) {
      l = pos;
    }
    pos = view.find('?');
    if (pos != std::string::npos && pos < l) {
      l = pos;
    }

    arangodb::velocypack::StringRef host(view.data(), l);
    size_t colon = host.find(':');
    if (colon == std::string::npos) {
      // no port
      result.host.populate(std::string(host.data(), host.size()));
    } else {
      result.host.populate(std::string(host.data(), colon));
      result.port.populate(std::string(host.data() + colon + 1, host.size() - colon - 1));

      if (!std::regex_match(result.port.value,
                            std::regex("^[0-9]+$", std::regex::nosubs | std::regex::ECMAScript))) {
        // port number must be numeric
        result.valid = false;
      }
    }

    if (!std::regex_match(result.host.value,
                          std::regex("^[a-zA-Z0-9\\-.]+$",
                                     std::regex::nosubs | std::regex::ECMAScript))) {
      // host pattern is invalid
      result.valid = false;
    }

    view = arangodb::velocypack::StringRef(host.data() + host.size());
  }

  // handle basedn
  if (!view.empty() && view[0] == '/') {
    // skip over "/"
    view = arangodb::velocypack::StringRef(view.data() + 1);

    size_t l = view.size();
    size_t pos = view.find('?');
    if (pos != std::string::npos) {
      l = pos;
    }

    arangodb::velocypack::StringRef basedn(view.data(), l);
    result.basedn.populate(std::string(basedn.data(), basedn.size()));
    if (basedn.empty()) {
      // basedn is empty
      result.valid = false;
    }

    if (basedn.find('/') != std::string::npos) {
      // basedn contains "/"
      result.valid = false;
    }

    view = arangodb::velocypack::StringRef(basedn.data() + basedn.size());
  } else {
    // if there is no basedn, we cannot have anything else
    if (!view.empty()) {
      // no basedn but trailing things in url
      result.valid = false;
    }
    return;
  }

  // handle searchAttribute
  if (!view.empty() && view[0] == '?') {
    // skip over "?"
    view = arangodb::velocypack::StringRef(view.data() + 1);

    size_t l = view.size();
    size_t pos = view.find('?');
    if (pos != std::string::npos) {
      l = pos;
    }

    arangodb::velocypack::StringRef searchAttribute(view.data(), l);
    result.searchAttribute.populate(
        std::string(searchAttribute.data(), searchAttribute.size()));
    if (result.searchAttribute.value.empty() ||
        !std::regex_match(result.searchAttribute.value,
                          std::regex("^[a-zA-Z0-9\\-_]+$",
                                     std::regex::nosubs | std::regex::ECMAScript))) {
      // search attribute pattern is invalid
      result.valid = false;
    }

    view = arangodb::velocypack::StringRef(searchAttribute.data() + searchAttribute.size());
  } else {
    // if there is no searchAttribute, there must not be anything else
    if (!view.empty()) {
      // no search attribute pattern, but trailing characters in string
      result.valid = false;
    }
    return;
  }

  // handle deep
  if (!view.empty() && view[0] == '?') {
    // skip over "?"
    view = arangodb::velocypack::StringRef(view.data() + 1);

    size_t l = view.size();
    size_t pos = view.find('?');
    if (pos != std::string::npos) {
      l = pos;
    }

    arangodb::velocypack::StringRef deep(view.data(), l);
    result.deep.populate(std::string(deep.data(), deep.size()));
    if (result.deep.value.empty() ||
        !std::regex_match(result.deep.value,
                          std::regex("^[a-zA-Z0-9\\-_]+$",
                                     std::regex::nosubs | std::regex::ECMAScript))) {
      // invalid deep pattern
      result.valid = false;
    }

    view = arangodb::velocypack::StringRef(deep.data() + deep.size());
  }

  // we must be at the end of the string here
  if (!view.empty()) {
    // trailing characters in string
    result.valid = false;
  }
}
