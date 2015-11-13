////////////////////////////////////////////////////////////////////////////////
/// @brief WAL and datafile markers
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Storage/Marker.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
      
// returns the static length for the marker type
// the static length is the total length of the marker's static data fields,
// excluding the base marker's fields and excluding the marker's dynamic
// VPack data values
uint64_t MarkerHelper::staticLength (MarkerType type) {
  switch (type) {
    case MarkerTypeHeader:
    case MarkerTypeFooter:
      return MarkerReaderMeta::staticLength();

    case MarkerTypeDocument:
    case MarkerTypeDocumentDeletion:
      return MarkerReaderDocument::staticLength();
    
    case MarkerTypeTransactionBegin:
    case MarkerTypeTransactionCommit:
    case MarkerTypeTransactionAbort:
      return MarkerReaderTransaction::staticLength();
    
    case MarkerTypeCollectionCreate:
    case MarkerTypeCollectionDrop:
    case MarkerTypeCollectionRename:
    case MarkerTypeCollectionChange:
      return MarkerReaderCollection::staticLength();
    
    case MarkerTypeIndexCreate:
    case MarkerTypeIndexDrop:
      return MarkerReaderIndex::staticLength();
    
    case MarkerTypeDatabaseCreate:
    case MarkerTypeDatabaseDrop:
      return MarkerReaderDatabase::staticLength();
    
    case MarkerMax:
      break;
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}
    
// calculate the required length for a marker of the specified type, given a
// payload of the specified length
uint64_t MarkerHelper::calculateMarkerLength (MarkerType type, uint64_t payloadLength) {
  uint64_t bodyLength = staticLength(type) + payloadLength; 
  return calculateHeaderLength(bodyLength) + bodyLength;
}

// calculate the required length for the header of a marker, given a body
// of the specified length
uint64_t MarkerHelper::calculateHeaderLength (uint64_t bodyLength) {
  if (bodyLength < (1 << (3 * 8))) {
    return 16;
  }
  return 24;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
