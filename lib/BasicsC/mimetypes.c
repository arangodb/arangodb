////////////////////////////////////////////////////////////////////////////////
/// @brief mimetypes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include "BasicsC/associative.h"
#include "BasicsC/hashes.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/voc-mimetypes.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Mimetypes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief mimetype
////////////////////////////////////////////////////////////////////////////////

typedef struct mimetype_s {
  char* _extension;
  char* _mimetype;
  bool  _appendCharset;
}
mimetype_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Mimetypes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static bool Initialised = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief the array of mimetypes
////////////////////////////////////////////////////////////////////////////////

static TRI_associative_pointer_t Mimetypes;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Mimetypes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash errors messages (not used)
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashMimetype (TRI_associative_pointer_t* array,
                              void const* element) {

  mimetype_t* entry = (mimetype_t*) element;
  return (uint64_t) TRI_FnvHashString(entry->_extension);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Comparison function used to determine error equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualMimetype (TRI_associative_pointer_t* array,
                           void const* key,
                           void const* element) {
  mimetype_t* entry = (mimetype_t*) element;

  return (strcmp((const char*) key, entry->_extension) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Mimetypes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief register a mimetype for an extension
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterMimetype (const char* extension, 
                           const char* mimetype,
                           bool appendCharset) {
  mimetype_t* entry;
  void* found;

  entry = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(mimetype_t), false);
  entry->_extension = TRI_DuplicateString(extension);
  entry->_appendCharset = appendCharset;

  if (appendCharset) {
    entry->_mimetype = TRI_Concatenate2String(mimetype, "; charset=utf-8");
  }
  else {
    entry->_mimetype = TRI_DuplicateString(mimetype);
  }

  found = TRI_InsertKeyAssociativePointer(&Mimetypes, extension, entry, false);

  return (found != NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the mimetype for an extension
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetMimetype (const char* extension) {
  mimetype_t* entry;
  
 entry = TRI_LookupByKeyAssociativePointer(&Mimetypes, (void const*) extension);

  if (entry == NULL) {
    return NULL;
  }

  return entry->_mimetype;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Mimetypes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the mimetypes
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseMimetypes () {
  if (Initialised) {
    return;
  }

  TRI_InitAssociativePointer(&Mimetypes,
                             TRI_CORE_MEM_ZONE,
                             &TRI_HashStringKeyAssociativePointer,
                             HashMimetype,
                             EqualMimetype,
                             0);

  TRI_InitialiseEntriesMimetypes();
  Initialised = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the mimetypes
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownMimetypes () {
  size_t i;

  if (! Initialised) {
    return;
  }

  for (i = 0; i < Mimetypes._nrAlloc; i++) {
    mimetype_t* mimetype = Mimetypes._table[i];

    if (mimetype != NULL) {
      TRI_Free(TRI_CORE_MEM_ZONE, mimetype->_extension);
      TRI_Free(TRI_CORE_MEM_ZONE, mimetype->_mimetype);
      TRI_Free(TRI_CORE_MEM_ZONE, mimetype);
    }
  }

  TRI_DestroyAssociativePointer(&Mimetypes);

  Initialised = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
