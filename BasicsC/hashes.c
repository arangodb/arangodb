////////////////////////////////////////////////////////////////////////////////
/// @brief hash functions
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

#include "hashes.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                               FNV
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                          public functions for FNV
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Hashes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for blobs
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashBlob (TRI_blob_t* blob) {
  return TRI_FnvHashPointer(blob->data, blob->length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for memory blobs
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashPointer (void const* buffer, size_t length) {
  uint64_t nMagicPrime;
  uint64_t nHashVal;
  uint8_t const* pFirst;
  uint8_t const* pLast;

  nMagicPrime = 0x00000100000001b3ULL;
  nHashVal = 0xcbf29ce484222325ULL;

  pFirst = (uint8_t const*) buffer;
  pLast = pFirst + length;

  while (pFirst < pLast) {
    nHashVal ^= *pFirst++;
    nHashVal *= nMagicPrime;
  }

  return nHashVal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for strings
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FnvHashString (char const* buffer) {
  uint64_t nMagicPrime;
  uint64_t nHashVal;
  uint8_t const* pFirst;

  nMagicPrime = 0x00000100000001b3ULL;
  nHashVal = 0xcbf29ce484222325ULL;

  pFirst = (uint8_t const*) buffer;

  while (*pFirst) {
    nHashVal ^= *pFirst++;
    nHashVal *= nMagicPrime;
  }

  return nHashVal;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             CRC32
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Hashes
/// @{
////////////////////////////////////////////////////////////////////////////////

static uint32_t Crc32Polynomial[256];

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Hashes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief CRC32 reflect
////////////////////////////////////////////////////////////////////////////////

static uint32_t ReflectCrc32 (uint32_t value, const int size) {
  int i;
  uint32_t reflected;

  // swap bit 0 for bit 7, bit 1 For bit 6, etc....
  reflected = 0;

  for (i = 1; i < (size + 1); ++i) {
    if (value & 1) {
      reflected = reflected | (1 << (size - i));
    }

    value = value >> 1;
  }

  return reflected;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates the CRC32 polynomial
////////////////////////////////////////////////////////////////////////////////

static void GenerateCrc32Polynomial (void) {
  int i;
  int j;
  uint32_t polynomial;

  // assign the CRC-32 bit polynomial
  polynomial = 0x04C11DB7;

  // fill the array of characters 256 values representing ASCII character codes.
  for (i = 0; i < 256; i++) {
    Crc32Polynomial[i] = ReflectCrc32(i, 8) << 24;

    for (j = 0; j < 8; j++) {
      Crc32Polynomial[i] = (Crc32Polynomial[i] << 1) ^ ((Crc32Polynomial[i] & (1 << 31)) ? polynomial : 0);
    }

    Crc32Polynomial[i] = ReflectCrc32(Crc32Polynomial[i], 32);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Hashes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initial CRC32 value
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_InitialCrc32 () {
  return (0xffffffff);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief final CRC32 value
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_FinalCrc32 (uint32_t value) {
  return (value ^ 0xffffffff);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief CRC32 value of data block
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_BlockCrc32 (uint32_t value, char const* data, size_t length) {
  uint8_t const* ptr;

  if (data == 0 || length == 0) {
    return value;
  }

  ptr = (uint8_t const*) data;

  while (length--) {
    value = (value >> 8) ^ (Crc32Polynomial[(value & 0xFF) ^ (*ptr)]);
    ++ptr;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief CRC32 value of data block ended by 0
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_BlockStringCrc32 (uint32_t value, char const* data) {
  uint8_t const* ptr;

  if (data == 0) {
    return value;
  }

  ptr = (uint8_t const*) data;

  while (*ptr != '\0') {
    value = (value >> 8) ^ (Crc32Polynomial[(value & 0xFF) ^ (*ptr)]);
    ++ptr;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a CRC32 for blobs
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_Crc32HashBlob (TRI_blob_t* blob) {
  return TRI_Crc32HashPointer(blob->data, blob->length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a CRC32 for memory blobs
////////////////////////////////////////////////////////////////////////////////

uint32_t TRI_Crc32HashPointer (void const* data, size_t length) {
  uint32_t crc;

  crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, data, length);

  return TRI_FinalCrc32(crc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a CRC32 for strings
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_Crc32HashString (char const* data) {
  uint32_t crc;
  uint8_t const* ptr;

  crc = TRI_InitialCrc32();
  ptr = (uint8_t const*) data;

  while (*data) {
    crc = (crc >> 8) ^ (Crc32Polynomial[(crc & 0xFF) ^ (*ptr)]);
    ++ptr;
  }

  return TRI_FinalCrc32(crc);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Hashes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static bool Initialised = false;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Hashes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the hashes components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseHashes () {
  if (Initialised) {
    return;
  }

  GenerateCrc32Polynomial();

  Initialised = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the hashes components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownHashes () {
  if (! Initialised) {
    return;
  }

  Initialised = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
