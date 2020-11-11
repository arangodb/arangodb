////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////
#include <string>
#include "Utilities/IsArangoExecuteabel.h"


bool HasShellExecuteableNameInIt(std::string& str) {
    bool foundShellTool = false;
    if (str.length() > 0 && str[0] == 'a') {
      auto pos = str.find(' ');
      if (pos != std::string::npos) {
        // is it a variable name assignment?
        if (str.find('=', pos) == std::string::npos) {
          str = str.substr(0, pos);
        }
      }
      if (str == "arangobackup" ||
          str == "arangobench" ||
          str == "arangod" ||
          str == "arangodbtests" ||
          str == "arangodump" ||
          str == "arangoexport" ||
          str == "arangoimp" ||
          str == "arangoimport" ||
          str == "arango-init-database" ||
          str == "arangoinspect" ||
          str == "arangorestore" ||
          str == "arango-secure-installation" ||
          str == "arangosh" ||
          str == "arangovpack"
          ) {
        foundShellTool = true;
      }
    }
    return foundShellTool;
}
