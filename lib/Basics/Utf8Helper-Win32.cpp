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
////////////////////////////////////////////////////////////////////////////////

#include <locale>
#include "Utf8Helper-Win32.h"

std::wstring arangodb::basics::toWString(std::string const& validUTF8String) {
  std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>> conv16;
  return conv16.from_bytes(validUTF8String);
}

std::string arangodb::basics::fromWString(wchar_t const* validUTF16String,
                                          std::size_t size) {
  std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>> conv16;
  return conv16.to_bytes(validUTF16String, validUTF16String + size);
}

std::string arangodb::basics::fromWString(
    std::wstring const& validUTF16String) {
  std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>> conv16;
  return conv16.to_bytes(validUTF16String);
}