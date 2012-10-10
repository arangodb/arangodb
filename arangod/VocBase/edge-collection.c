////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
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

#include "edge-collection.h"

//#include "BasicsC/conversions.h"
//#include "BasicsC/files.h"
//#include "BasicsC/hashes.h"
//#include "BasicsC/logging.h"
//#include "BasicsC/strings.h"
//#include "ShapedJson/shape-accessor.h"
//#include "VocBase/index.h"
//#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupEdgesDocumentCollection (TRI_document_collection_t* edges,
                                                        TRI_edge_direction_e direction,
                                                        TRI_voc_cid_t cid,
                                                        TRI_voc_did_t did) {
  union { TRI_doc_mptr_t* v; TRI_doc_mptr_t const* c; } cnv;
  TRI_vector_pointer_t result;
  TRI_edge_header_t entry;
  TRI_vector_pointer_t found;
  size_t i;

  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  entry._direction = direction;
  entry._cid = cid;
  entry._did = did;

  found = TRI_LookupByKeyMultiPointer(TRI_UNKNOWN_MEM_ZONE, &edges->_edgesIndex, &entry);

  for (i = 0;  i < found._length;  ++i) {
    cnv.c = ((TRI_edge_header_t*) found._buffer[i])->_mptr;

    TRI_PushBackVectorPointer(&result, cnv.v);
  }

  TRI_DestroyVectorPointer(&found);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
