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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <unordered_map>

#include "Translator.h"

#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"

namespace {
std::unordered_map<std::string, std::string> environment;
}

void arangodb::options::DefineEnvironment(std::string const& keyValues) {
  std::vector<std::string> kvs = basics::StringUtils::split(keyValues, ',');

  for (auto const& keyValue : kvs) {
    std::string key;
    std::string value;

    size_t const delim = keyValue.find_first_of('=');

    if (delim == std::string::npos) {
      key = keyValue;
    } else {
      key = keyValue.substr(0, delim);
      value = keyValue.substr(delim + 1);
    }

    environment[key] = value;
  }
}

std::string arangodb::options::EnvironmentTranslator(std::string const& value,
                                                     char const* binaryPath) {
  if (value.empty()) {
    return value;
  }

  char const* p = value.c_str();
  char const* e = p + value.size();

  std::string result;

  for (char const* q = p; q < e; q++) {
    if (*q == '@') {
      q++;

      if (*q == '@') {
        result.push_back('@');
      } else {
        char const* t = q;

        for (; q < e && *q != '@'; q++)
          ;

        if (q < e) {
          std::string k = std::string(t, q);
          std::string vv;
          if (!TRI_GETENV(k.c_str(), vv) || (vv.length() == 0)) {
            auto iter = environment.find(k);

            if (iter != environment.end()) {
              vv = iter->second;
            }
          }

          if (vv.length() == 0) {
            if (k == "PID") {
              vv = std::to_string(Thread::currentProcessId());
            }
#if _WIN32
            else if (k == "ROOTDIR") {
              vv = TRI_LocateInstallDirectory(nullptr, binaryPath);

              if (!vv.empty()) {
                char c = *(vv.rbegin());

                if (c == TRI_DIR_SEPARATOR_CHAR || c == '/') {
                  vv.pop_back();
                }
              }
            }
#endif
          }

          result += vv;
        } else {
          result += std::string(t - 1);
        }
      }
    } else {
      result.push_back(*q);
    }
  }

  return result;
}
