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

#ifndef ARANGODB_BASICS_MIMETYPES_H
#define ARANGODB_BASICS_MIMETYPES_H 1

/// @brief register a mimetype for an extension
bool TRI_RegisterMimetype(char const*, char const*, bool);

/// @brief gets the mimetype for an extension
char const* TRI_GetMimetype(char const*);

/// @brief initializes mimetypes
void TRI_InitializeMimetypes();

/// @brief shuts down mimetypes
void TRI_ShutdownMimetypes();

#endif
