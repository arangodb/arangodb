////////////////////////////////////////////////////////////////////////////////
/// @brief abstraction for libbson
///
/// @file lib/BasicsC/tri-bson.h
///
/// DISCLAIMER
///
/// Copyright 2014-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_TRI_BSON_H
#define TRIAGENS_BASICS_C_TRI_BSON_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "libbson-1.0/bson.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                            types
// -----------------------------------------------------------------------------

typedef bson_t TRI_bson_t;

static inline void TRI_bson_init (TRI_bson_t* b) {
  bson_init(b);
}

static inline void TRI_bson_destroy (TRI_bson_t* b) {
  bson_destroy(b);
}

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


