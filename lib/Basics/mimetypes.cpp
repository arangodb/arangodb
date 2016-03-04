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

#include "Basics/Common.h"
#include "Basics/voc-mimetypes.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialized
////////////////////////////////////////////////////////////////////////////////

static bool Initialized = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief the array of mimetypes
////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<std::string, std::string> Mimetypes;

////////////////////////////////////////////////////////////////////////////////
/// @brief register a mimetype for an extension
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterMimetype(char const* extension, char const* mimetype,
                          bool appendCharset) {
  std::string full(mimetype);
  if (appendCharset) {
    full.append("; charset=utf-8");
  }

  return !Mimetypes.emplace(std::string(extension), full).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the mimetype for an extension
////////////////////////////////////////////////////////////////////////////////

char const* TRI_GetMimetype(char const* extension) {
  auto it = Mimetypes.find(std::string(extension));

  if (it == Mimetypes.end()) {
    return nullptr;
  }

  return (*it).second.c_str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the mimetypes
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeMimetypes() {
  if (Initialized) {
    return;
  }

  TRI_InitializeEntriesMimetypes();
  Initialized = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the mimetypes
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownMimetypes() {
  Initialized = false;
}
