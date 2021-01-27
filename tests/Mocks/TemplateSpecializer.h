////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhöffer
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_TESTS_MOCKS_TEMPLATE_SPECIALIZER_H
#define ARANGODB_TESTS_MOCKS_TEMPLATE_SPECIALIZER_H 1

namespace {

char const* plan_dbs_string =
#include "plan_dbs_db.h"
    ;

char const* plan_colls_string =
#include "plan_colls_db.h"
    ;

char const* current_dbs_string =
#include "current_dbs_db.h"
    ;

char const* current_colls_string =
#include "current_colls_db.h"
    ;

class TemplateSpecializer {
  std::unordered_map<std::string, std::string> _replacements;
  int _nextServerNumber;
  std::string _dbName;

  enum ReplacementCase { Not, Number, Shard, DBServer, DBName };

 public:
  TemplateSpecializer(std::string const& dbName)
      : _nextServerNumber(1), _dbName(dbName) {}

  std::string specialize(char const* templ) {
    size_t len = strlen(templ);

    size_t pos = 0;
    std::string result;
    result.reserve(len);

    while (pos < len) {
      if (templ[pos] != '"') {
        result.push_back(templ[pos++]);
      } else {
        std::string st;
        pos = parseString(templ, pos, len, st);
        ReplacementCase c = whichCase(st);
        if (c != ReplacementCase::Not) {
          auto it = _replacements.find(st);
          if (it != _replacements.end()) {
            st = it->second;
          } else {
            std::string newSt;
            switch (c) {
              case ReplacementCase::Number:
                newSt = std::to_string(TRI_NewTickServer());
                break;
              case ReplacementCase::Shard:
                newSt = std::string("s") + std::to_string(TRI_NewTickServer());
                break;
              case ReplacementCase::DBServer:
                newSt = std::string("PRMR_000") + std::to_string(_nextServerNumber++);
                break;
              case ReplacementCase::DBName:
                newSt = _dbName;
                break;
              case ReplacementCase::Not:
                newSt = st;
                // never happens, just to please compilers
            }
            _replacements[st] = newSt;
            st = newSt;
          }
        }
        result.push_back('"');
        result.append(st);
        result.push_back('"');
      }
    }
    return result;
  }

 private:
  size_t parseString(char const* templ, size_t pos, size_t const len, std::string& st) {
    // This must be called when templ[pos] == '"'. It parses the string
    // and // puts it into st (not including the quotes around it).
    // The return value is pos advanced to behind the closing quote of
    // the string.
    ++pos;  // skip quotes
    size_t startPos = pos;
    while (pos < len && templ[pos] != '"') {
      ++pos;
    }
    // Now the string in question is between startPos and pos.
    // Extract string as it is:
    st = std::string(templ + startPos, pos - startPos);
    // Skip final quotes if they are there:
    if (pos < len && templ[pos] == '"') {
      ++pos;
    }
    return pos;
  }

  ReplacementCase whichCase(std::string& st) {
    bool onlyNumbers = true;
    size_t pos = 0;
    if (st == "db") {
      return ReplacementCase::DBName;
    }
    ReplacementCase c = ReplacementCase::Number;
    if (pos < st.size() && st[pos] == 's') {
      c = ReplacementCase::Shard;
      ++pos;
    } else if (pos + 5 <= st.size() && st.substr(0, 5) == "PRMR-") {
      return ReplacementCase::DBServer;
    }
    for (; pos < st.size(); ++pos) {
      if (st[pos] < '0' || st[pos] > '9') {
        onlyNumbers = false;
        break;
      }
    }
    if (!onlyNumbers) {
      return ReplacementCase::Not;
    }
    return c;
  }
};

}  // namespace

#endif
