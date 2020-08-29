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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_COMMON_H
#define ARANGODB_BASICS_COMMON_H 1

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif

#ifdef _WIN32
// some Windows headers define macros named free and small,
// leading to follow-up compile errors
#undef free
#undef small
// Windows debug mode also seems to define DEBUG preproc symbol
#undef DEBUG
#endif

#ifdef ARANGODB_USE_GOOGLE_TESTS
#define TEST_VIRTUAL virtual
#else
#define TEST_VIRTUAL
#endif

#ifdef sleep
#undef sleep
#endif
#define sleep ERROR_USE_std_this_thread_sleep_for

#endif
