////////////////////////////////////////////////////////////////////////////////
/// @brief High-Performance Database Framework made by triagens
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/Common.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                              BLOB
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup BasicStructures
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief destorys the data of blob, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBlob (TRI_blob_t* blob) {
  if (blob != NULL) {
    if (blob->data != NULL) {
      TRI_Free(blob->data);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destorys the data of blob and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBlob (TRI_blob_t* blob) {
  if (blob != NULL) {
    TRI_DestroyBlob(blob);
    TRI_Free(blob);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup BasicStructures
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a blob
////////////////////////////////////////////////////////////////////////////////

TRI_blob_t* TRI_CopyBlob (TRI_blob_t const* src) {
  TRI_blob_t* dst;

  dst = (TRI_blob_t*) TRI_Allocate(sizeof(TRI_blob_t));
  dst->length = src->length;

  if (src->length == 0 || src->data == NULL) {
    dst->length = 0;
    dst->data = NULL;
  }
  else {
    dst->length = src->length;
    dst->data = TRI_Allocate(dst->length);
    memcpy(dst->data, src->data, src->length);
  }

  return dst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a blob into given destination
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyToBlob (TRI_blob_t* dst, TRI_blob_t const* src) {
  dst->length = src->length;

  if (src->length == 0 || src->data == NULL) {
    dst->length = 0;
    dst->data = NULL;
  }
  else {
    dst->length = src->length;
    dst->data = TRI_Allocate(dst->length);
    memcpy(dst->data, src->data, src->length);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
