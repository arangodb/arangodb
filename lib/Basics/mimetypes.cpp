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

#include "Basics/associative.h"
#include "Basics/hashes.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-mimetypes.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief mimetype
////////////////////////////////////////////////////////////////////////////////

typedef struct mimetype_s {
  char* _extension;
  char* _mimetype;
  bool _appendCharset;
} mimetype_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialized
////////////////////////////////////////////////////////////////////////////////

static bool Initialized = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief the array of mimetypes
////////////////////////////////////////////////////////////////////////////////

static TRI_associative_pointer_t Mimetypes;

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash errors messages (not used)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashMimetype(TRI_associative_pointer_t* array,
                             void const* element) {
  mimetype_t* entry = (mimetype_t*)element;
  return (uint64_t)TRI_FnvHashString(entry->_extension);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Comparison function used to determine error equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualMimetype(TRI_associative_pointer_t* array, void const* key,
                          void const* element) {
  mimetype_t* entry = (mimetype_t*)element;

  return (strcmp((char const*)key, entry->_extension) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a mimetype for an extension
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterMimetype(char const* extension, char const* mimetype,
                          bool appendCharset) {
  mimetype_t* entry = static_cast<mimetype_t*>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(mimetype_t), false));
  entry->_extension = TRI_DuplicateString(extension);
  entry->_appendCharset = appendCharset;

  if (appendCharset) {
    entry->_mimetype = TRI_Concatenate2String(mimetype, "; charset=utf-8");
  } else {
    entry->_mimetype = TRI_DuplicateString(mimetype);
  }

  void* found =
      TRI_InsertKeyAssociativePointer(&Mimetypes, extension, entry, false);

  return (found != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the mimetype for an extension
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetMimetype(char const* extension) {
  mimetype_t* entry = static_cast<mimetype_t*>(
      TRI_LookupByKeyAssociativePointer(&Mimetypes, (void const*)extension));

  if (entry == nullptr) {
    return nullptr;
  }

  return entry->_mimetype;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the mimetypes
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeMimetypes() {
  if (Initialized) {
    return;
  }

  TRI_InitAssociativePointer(&Mimetypes, TRI_CORE_MEM_ZONE,
                             &TRI_HashStringKeyAssociativePointer, HashMimetype,
                             EqualMimetype, 0);

  TRI_InitializeEntriesMimetypes();
  Initialized = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the mimetypes
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownMimetypes() {
  size_t i;

  if (!Initialized) {
    return;
  }

  for (i = 0; i < Mimetypes._nrAlloc; i++) {
    mimetype_t* mimetype = static_cast<mimetype_t*>(Mimetypes._table[i]);

    if (mimetype != nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, mimetype->_extension);
      TRI_Free(TRI_CORE_MEM_ZONE, mimetype->_mimetype);
      TRI_Free(TRI_CORE_MEM_ZONE, mimetype);
    }
  }

  TRI_DestroyAssociativePointer(&Mimetypes);

  Initialized = false;
}
