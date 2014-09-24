////////////////////////////////////////////////////////////////////////////////
/// @brief hash functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_HASHES_H
#define ARANGODB_BASICS_C_HASHES_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--        (FNV-1a Fowler–Noll–Vo hash function)                  FNV
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for blocks
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashBlock (uint64_t, void const*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for memory blobs
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashPointer (void const*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for blobs
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashBlob (TRI_blob_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for strings
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashString (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a initial FNV for blocks
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashBlockInitial (void);

// -----------------------------------------------------------------------------
// --SECTION--                                                             CRC32
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initial CRC32 value
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_InitialCrc32 (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief final CRC32 value
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_FinalCrc32 (uint32_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief CRC32 value of data block
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_BlockCrc32 (uint32_t, char const* data, size_t length);

////////////////////////////////////////////////////////////////////////////////
/// @brief CRC32 value of data block ended by 0
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_BlockStringCrc32 (uint32_t, char const* data);

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a CRC32 for blobs
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_Crc32HashBlob (TRI_blob_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a CRC32 for memory blobs
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_Crc32HashPointer (void const*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a CRC32 for strings
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_Crc32HashString (char const*);

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the hashes components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseHashes (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the hashes components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownHashes (void);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
