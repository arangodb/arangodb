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

#ifndef ARANGODB_BASICS_TRI__ZIP_H
#define ARANGODB_BASICS_TRI__ZIP_H 1

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <cstdint>
#include <string>
#include <vector>

#include "Basics/Common.h"
#include "ErrorCode.h"

ErrorCode TRI_Adler32(char const* filename, uint32_t& checksum);

////////////////////////////////////////////////////////////////////////////////
/// @brief zips a file
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_ZipFile(char const* filename, char const* dir,
                      std::vector<std::string> const& files, char const* password);

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a file
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_UnzipFile(char const* filename, char const* outPath, bool skipPaths,
                        bool overwrite, char const* password, std::string& errorMessage);

#endif
