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

#include "Storage/Marker.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
  
// returns a type name for a marker
char const* MarkerHelper::typeName(MarkerType type) {
  switch (type) {
    case MarkerTypeHeader:
      return "header";
    case MarkerTypeFooter:
      return "footer";
    case MarkerTypeDocumentPreface:
      return "document preface";
    case MarkerTypeDocument:
      return "document";
    case MarkerTypeDocumentDeletion:
      return "document deletion";
    case MarkerTypeTransactionBegin:
      return "transaction begin";
    case MarkerTypeTransactionCommit:
      return "transaction commit";
    case MarkerTypeTransactionAbort:
      return "transaction abort";
    case MarkerTypeCollectionCreate:
      return "collection create";
    case MarkerTypeCollectionDrop:
      return "collection drop";
    case MarkerTypeCollectionRename:
      return "collection rename";
    case MarkerTypeCollectionProperties:
      return "collection properties";
    case MarkerTypeIndexCreate:
      return "index create";
    case MarkerTypeIndexDrop:
      return "index drop";
    case MarkerTypeDatabaseCreate:
      return "database create";
    case MarkerTypeDatabaseDrop:
      return "database drop";
    case MarkerMax:
      break;
  }

  return "invalid marker type";
}

// returns the static length for the marker type
// the static length is the total length of the marker's static data fields,
// excluding the base marker's fields and excluding the marker's dynamic
// VPack data values
uint64_t MarkerHelper::staticLength(MarkerType type) {
  switch (type) {
    case MarkerTypeHeader:
    case MarkerTypeFooter:
      return MarkerReaderMeta::staticLength();

    case MarkerTypeDocumentPreface:
      return MarkerReaderDocumentPreface::staticLength();

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
    case MarkerTypeCollectionProperties:
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
uint64_t MarkerHelper::calculateMarkerLength(MarkerType type,
                                             uint64_t payloadLength) {
  uint64_t bodyLength = staticLength(type) + payloadLength;
  return calculateHeaderLength(bodyLength) + bodyLength;
}

// calculate the required length for the header of a marker, given a body
// of the specified length
uint64_t MarkerHelper::calculateHeaderLength(uint64_t bodyLength) {
  if (bodyLength < (1 << (3 * 8))) {
    return 16;
  }
  return 24;
}

std::ostream& operator<<(std::ostream& stream,
                         arangodb::MarkerReader const* marker) {
  stream << "[Marker " << MarkerHelper::typeName(marker->type())
         << ", size: " << marker->length() << "]";
  return stream;
}

