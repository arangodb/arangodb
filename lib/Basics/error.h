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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ERROR_H
#define ARANGODB_BASICS_ERROR_H 1

#include <string_view>

#include "Basics/ErrorCode.h"

/// @brief returns the last error
ErrorCode TRI_errno();

/// @brief returns the last error as string
std::string_view TRI_last_error();

/// @brief sets the last error
ErrorCode TRI_set_errno(ErrorCode);

/// @brief defines an error string
void TRI_set_errno_string(ErrorCode code, char const* msg);

/// @brief return an error message for an error code
<<<<<<< HEAD
std::string_view TRI_errno_string(int code) noexcept;
=======
char const* TRI_errno_string(ErrorCode code) noexcept;
>>>>>>> 2fcec28799eec22959cd24855b891bb929539f39

/// @brief initializes the error messages
void TRI_InitializeError();

#endif
