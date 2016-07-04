////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Translator.h"
#include "Basics/tri-strings.h"
#include "Basics/files.h"
std::string arangodb::options::EnvironmentTranslator(std::string const& value) {
  if (value.empty()) {
    return value;
  }

  char const* p = value.c_str();
  char const* e = p + value.size();

  std::string result;

  for (char const* q = p;  q < e;  q++) {
    if (*q == '@') {
      q++;

      if (*q == '@') {
        result.push_back('@');
      }
      else {
        char const* t = q;

        for (;  q < e && *q != '@';  q++) ;

        if (q < e) {
          std::string k = std::string(t, q);
          char* v = getenv(k.c_str());
          std::string vv;

          if (v != nullptr && *v == '\0') {
            v = nullptr;
          }

          if (v == nullptr) {
#if _WIN32
            if (TRI_EqualString(k.c_str(), "ROOTDIR")) {
              vv = TRI_LocateInstallDirectory();

              if (! vv.empty()) {
                char c = *(vv.rbegin());

                if (c == TRI_DIR_SEPARATOR_CHAR || c == '/') {
                  vv.pop_back();
                }
              }
            }
#endif
          }
          else {
            vv = v;
          }

          result += vv;
        }
        else {
          result += std::string(t - 1);
        }
      }
    }
    else {
      result.push_back(*q);
    }
  }

  return result;
}
