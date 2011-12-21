////////////////////////////////////////////////////////////////////////////////
/// @brief blob collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_BLOB_COLLECTION_H
#define TRIAGENS_DURHAM_VOC_BASE_BLOB_COLLECTION_H 1

#include <BasicsC/common.h>

#include <VocBase/vocbase.h>
#include <VocBase/collection.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief blob collection
///
/// A blob collection is a collection of blobs. There is no versioning or
/// relationship between the blobs. The data is directly synced to disks.
/// Therefore no special management thread is needed. It is not possible to
/// delete entries, once they are created. The only query supported is a
/// full scan.
///
/// Calls to @ref TRI_WriteBlobCollection are synchronised using the _lock
/// of a blob collection.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_blob_collection_s {
  TRI_collection_t base;

  TRI_mutex_t _lock;
}
TRI_blob_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_blob_collection_t* TRI_CreateBlobCollection (char const* path,
                                                 TRI_col_parameter_t* parameter);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBlobCollection (TRI_blob_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBlobCollection (TRI_blob_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief writes an element splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

bool TRI_WriteBlobCollection (TRI_blob_collection_t* collection,
                              TRI_df_marker_t* marker,
                              TRI_voc_size_t markerSize,
                              void const* body,
                              TRI_voc_size_t bodySize,
                              TRI_df_marker_t** result);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_blob_collection_t* TRI_OpenBlobCollection (char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseBlobCollection (TRI_blob_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

