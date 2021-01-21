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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_LDAP_URL_PARSER_H
#define ARANGODB_BASICS_LDAP_URL_PARSER_H 1

#include <string>

#include "Basics/Common.h"

namespace arangodb {

struct LdapUrlParseResultComponent {
  LdapUrlParseResultComponent() : set(false) {}
  explicit LdapUrlParseResultComponent(std::string const& defaultValue)
      : value(defaultValue), set(false) {}

  void populate(std::string const& newValue) {
    value = newValue;
    set = true;
  }

  void populate(std::string&& newValue) {
    value = std::move(newValue);
    set = true;
  }

  std::string value;
  bool set;
};

struct LdapUrlParseResult {
  LdapUrlParseResult() : valid(false) {}

  LdapUrlParseResultComponent protocol;
  LdapUrlParseResultComponent host;
  LdapUrlParseResultComponent port;
  LdapUrlParseResultComponent basedn;
  LdapUrlParseResultComponent searchAttribute;
  LdapUrlParseResultComponent deep;

  std::string toString() const;

  bool valid;
};

class LdapUrlParser {
 public:
  static LdapUrlParseResult parse(std::string const& url);
  static void parse(std::string const& url, LdapUrlParseResult& result);
};

}  // namespace arangodb

#endif
