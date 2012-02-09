////////////////////////////////////////////////////////////////////////////////
/// @brief data-feeder
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

#include "VocBase/data-feeder.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief init select data feeder
////////////////////////////////////////////////////////////////////////////////

static void InitFeeder (TRI_data_feeder_t* feeder) {
  TRI_sim_collection_t* collection;
  TRI_doc_mptr_t* document;

  collection = (TRI_sim_collection_t*) feeder->_collection;
  if (collection->_primaryIndex._nrAlloc == 0) {
    feeder->_start   = NULL;
    feeder->_end     = NULL;
    feeder->_current = NULL;
    return;
  }

  feeder->_start = (void**) collection->_primaryIndex._table;
  feeder->_end   = (void**) (feeder->_start + collection->_primaryIndex._nrAlloc);

  // collections contain documents in a hash table
  // some of the entries are empty, and some contain deleted documents
  // it is invalid to use every entry from the hash table but the invalid documents
  // must be skipped.
  // adjust starts to first valid document in collection
  while (feeder->_start < feeder->_end) {
    document = (TRI_doc_mptr_t*) *(feeder->_start);
    if (document != NULL && !document->_deletion) {
      break;
    }

    feeder->_start++;
  }

  // iterate from end of document hash table to front and skip all documents
  // that are either deleted or empty
  while (feeder->_end > feeder->_start) {
    document = (TRI_doc_mptr_t*) *(feeder->_end - 1);
    if (document != NULL && !document->_deletion) {
      break;
    }
    feeder->_end--;
  }

  feeder->rewind(feeder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rewind select data feeder
////////////////////////////////////////////////////////////////////////////////

static void RewindFeeder (TRI_data_feeder_t* feeder) {
  feeder->_current = feeder->_start;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get current item from data feeder and advance next pointer
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t* CurrentFeeder (TRI_data_feeder_t* feeder) {
  TRI_doc_mptr_t* document;

  while (feeder->_current < feeder->_end) {
    document = (TRI_doc_mptr_t*) *(feeder->_current);
    feeder->_current++;
    if (document && document->_deletion == 0) {
      return document;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if data feeder is at eof
////////////////////////////////////////////////////////////////////////////////

bool EofFeeder (TRI_data_feeder_t* feeder) {
  return (feeder->_current < feeder->_end);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free select data feeder
////////////////////////////////////////////////////////////////////////////////

static void FreeFeeder (TRI_data_feeder_t* feeder) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new data feeder
////////////////////////////////////////////////////////////////////////////////

TRI_data_feeder_t* TRI_CreateDataFeeder (const TRI_doc_collection_t* collection) {
  TRI_data_feeder_t* feeder;

  feeder = (TRI_data_feeder_t*) TRI_Allocate(sizeof(TRI_data_feeder_t));
  if (!feeder) {
    return NULL;
  }

  feeder->_collection = collection;

  feeder->init        = InitFeeder;
  feeder->rewind      = RewindFeeder;
  feeder->current     = CurrentFeeder;
  feeder->eof         = EofFeeder;
  feeder->free        = FreeFeeder;

  feeder->init(feeder);

  return feeder;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

