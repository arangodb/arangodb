////////////////////////////////////////////////////////////////////////////////
/// @brief index iterator
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "index-iterator.h"



// -----------------------------------------------------------------------------
// --SECTION--                                             common public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup indexIterator
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// TODO: add some generic callback methods, and add a CreateIndexIterator
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief Destroys an index iterator witout freeing it
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyIndexIterator(TRI_index_iterator_t* iterator) {

  // ...........................................................................
  // Check if we have a valid iterator
  // ...........................................................................

  if (iterator == NULL) {
    return;
  }


  // ...........................................................................
  // Since we have no idea what the structures mean for an iterator (it depends
  // on what/who created the iterator), we simply callback the appropriate
  // destroy function.
  // ...........................................................................

  if (iterator->_destroyIterator != NULL) {
    iterator->_destroyIterator(iterator);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Destroys an index iterator and frees the allocated memory
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexIterator(TRI_index_iterator_t* iterator) {

  // ...........................................................................
  // Check if we have a valid iterator
  // ...........................................................................

  if (iterator == NULL) {
    return;
  }


  TRI_DestroyIndexIterator(iterator);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, iterator);
}




////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:


