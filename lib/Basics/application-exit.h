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
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASICS_APPLICATION_EXIT_H
#define LIB_BASICS_APPLICATION_EXIT_H 1

#ifndef TRI_WITHIN_COMMON
#error use <Basics/Common.h>
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief exit function type
////////////////////////////////////////////////////////////////////////////////

typedef void (*TRI_ExitFunction_t)(int, void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief exit function
////////////////////////////////////////////////////////////////////////////////

extern TRI_ExitFunction_t TRI_EXIT_FUNCTION;

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a exit function
////////////////////////////////////////////////////////////////////////////////

void TRI_Application_Exit_SetExit(TRI_ExitFunction_t);

#endif
