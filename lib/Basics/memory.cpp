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

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#include <cstring>
#endif

#include <cstdlib>

#include "Basics/Common.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"

/// @brief basic memory management for allocate
void* TRI_Allocate(size_t n) {
  void* m = ::malloc(n);

  if (m == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // prefill with 0xA5 (magic value, same as Valgrind will use)
  memset(m, 0xA5, n);
#endif

  return m;
}

/// @brief basic memory management for reallocate
void* TRI_Reallocate(void* m, size_t n) {
  if (m == nullptr) {
    return TRI_Allocate(n);
  }

  void* p = ::realloc(m, n);

  if (p == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  return p;
}

/// @brief basic memory management for deallocate
void TRI_Free(void* m) { ::free(m); }
