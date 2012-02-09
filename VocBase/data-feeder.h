////////////////////////////////////////////////////////////////////////////////
/// @brief data feeder for selects
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_DATA_FEEDER_H
#define TRIAGENS_DURHAM_VOC_BASE_DATA_FEEDER_H 1

#include "VocBase/simple-collection.h"
#include "VocBase/result.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief data feeder for selects
///
/// A data feeder is used to access the documents in a collection sequentially.
/// The documents are accessed in order of definition in the collection's hash
/// table. The hash table might also contain empty entries (nil pointers) or
/// deleted documents. The data feeder abstracts all this and provides easy 
/// access to all (relevant) documents in the hash table. 
/// For each collection in a join, one data feeder will be used. If a collection 
/// is invoked multiple times in a select (e.g. A INNER JOIN A) then there will
/// be multiple data feeders (in this case for collection A). This is because
/// the data feeder also contains state information (current position) that is
/// distinct for multiple instances in the same join
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_data_feeder_s {
  const TRI_doc_collection_t* _collection;
  void **_start;
  void **_end;
  void **_current;
  
  void (*init) (struct TRI_data_feeder_s*);
  void (*rewind) (struct TRI_data_feeder_s*);
  TRI_doc_mptr_t* (*current) (struct TRI_data_feeder_s*);
  bool (*eof) (struct TRI_data_feeder_s*);
  void (*free) (struct TRI_data_feeder_s*);
}
TRI_data_feeder_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new data feeder
////////////////////////////////////////////////////////////////////////////////

TRI_data_feeder_t* TRI_CreateDataFeeder(const TRI_doc_collection_t*);

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

