////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

#include <windows.h>
#include <shellapi.h>

#include <unicode/unistr.h>

static std::vector<std::string> argVec;

void TRI_GET_ARGV_WIN(int& argc, char** argv) {
  auto wargStr = GetCommandLineW();

  // if you want your argc in unicode, all you gonna do
  // is ask:
  auto wargv = CommandLineToArgvW(wargStr, &argc);

  argVec.reserve(argc);

  icu::UnicodeString buf;
  std::string uBuf;
  for (int i = 0; i < argc; i++) {
    uBuf.clear();
    // convert one UTF16 argument to utf8:
    buf = wargv[i];
    buf.toUTF8String<std::string>(uBuf);
    // memorize the utf8 value to keep the instance:
    argVec.push_back(uBuf);

    // Now overwrite our original argc entry with the utf8 one:
    argv[i] = (char*)argVec[i].c_str();
  }
}
