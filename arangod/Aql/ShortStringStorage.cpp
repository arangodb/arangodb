////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, short string storage
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ShortStringStorage.h"
#include "Basics/Exceptions.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                               static initializers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum length of a "short" string
////////////////////////////////////////////////////////////////////////////////
        
size_t const ShortStringStorage::MaxStringLength = 127;
      
// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a short string storage instance
////////////////////////////////////////////////////////////////////////////////

ShortStringStorage::ShortStringStorage (size_t blockSize) 
  : _blocks(),
    _blockSize(blockSize),
    _current(nullptr),
    _end(nullptr) {

  TRI_ASSERT(blockSize >= 64);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a short string storage instance
////////////////////////////////////////////////////////////////////////////////

ShortStringStorage::~ShortStringStorage () {
  for (auto& it : _blocks) {
    delete[] it;
  }      
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief register a short string
////////////////////////////////////////////////////////////////////////////////

char* ShortStringStorage::registerString (char const* p,
                                          size_t length) {
  TRI_ASSERT_EXPENSIVE(length <= MaxStringLength);

  if (_current == nullptr || (_current + length + 1 > _end)) {
    allocateBlock();
  }

  TRI_ASSERT_EXPENSIVE(! _blocks.empty());
  TRI_ASSERT_EXPENSIVE(_current != nullptr);
  TRI_ASSERT_EXPENSIVE(_end != nullptr);
  TRI_ASSERT_EXPENSIVE(_current + length + 1 <= _end);

  char* position = _current;
  memcpy(static_cast<void*>(position), p, length);
  _current[length] = '\0';
  _current += length + 1;

  return position;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate a new block of memory
////////////////////////////////////////////////////////////////////////////////

void ShortStringStorage::allocateBlock () {
  char* buffer = new char[_blockSize];

  try {
    _blocks.emplace_back(buffer);
    _current = buffer;
    _end     = _current + _blockSize;
  }
  catch (...) {
    delete[] buffer;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
