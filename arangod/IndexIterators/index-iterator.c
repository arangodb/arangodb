////////////////////////////////////////////////////////////////////////////////
/// @brief index iterator
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
/// @author Dr. O
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


