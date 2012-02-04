////////////////////////////////////////////////////////////////////////////////
/// @brief joins
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

#include "join.h"


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief free join data feeder
////////////////////////////////////////////////////////////////////////////////

static void FreeFeeder(TRI_join_feeder_t *feeder) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init join data feeder
////////////////////////////////////////////////////////////////////////////////

static void InitFeeder(TRI_join_feeder_t *feeder) {
  TRI_sim_collection_t *collection;
  TRI_doc_mptr_t *document;

  collection = (TRI_sim_collection_t *) feeder->_collection;
  if (collection->_primaryIndex._nrAlloc == 0) {
    feeder->_start = 0;
    feeder->_end = 0;
    feeder->_current = 0;
    return;
  }

  feeder->_start = (void **) collection->_primaryIndex._table;
  feeder->_end   = (void **) (feeder->_start + collection->_primaryIndex._nrAlloc);

  // collections contain documents in a hash table
  // some of the entries are empty, and some contain deleted documents
  // it is invalid to use every entry from the hash table but the invalid documents
  // must be skipped.
  // adjust starts to first valid document in collection
  while (feeder->_start < feeder->_end) {
    document = (TRI_doc_mptr_t*) *(feeder->_start);
    if (document != 0 && !document->_deletion) {
      break;
    }
    feeder->_start++;
  }

  while (feeder->_end > feeder->_start) {
    document = (TRI_doc_mptr_t*) *(feeder->_end - 1);
    if (document != 0 && !document->_deletion) {
      break;
    }
    feeder->_end--;
  }
  feeder->rewind(feeder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rewind join data feeder
////////////////////////////////////////////////////////////////////////////////

static void RewindFeeder(TRI_join_feeder_t *feeder) {
  feeder->_current = feeder->_start;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get current item from join data feeder and advance next pointer
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t *CurrentFeeder(TRI_join_feeder_t *feeder) {
  TRI_doc_mptr_t *document;

  while (feeder->_current < feeder->_end) {
    document = (TRI_doc_mptr_t*) *(feeder->_current);
    feeder->_current++;
    if (document && document->_deletion == 0) {
      return document;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get next item from join data feeder and advance next pointer
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t *NextFeeder(TRI_join_feeder_t *feeder) {
  TRI_doc_mptr_t *document;

  while (feeder->_current < feeder->_end) {
    feeder->_current++;
    document = (TRI_doc_mptr_t*) *(feeder->_current);
    if (document && document->_deletion == 0) {
      return document;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a join part
////////////////////////////////////////////////////////////////////////////////

static void FreePartJoin(TRI_join_part_t *part) {
  part->_feeder->free(part->_feeder);
  TRI_Free(part->_feeder);
  TRI_Free(part);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a join part to a join
////////////////////////////////////////////////////////////////////////////////

void TRI_AddPartJoin(TRI_join_t *join, const TRI_join_type_e type, TRI_resultset_t *resultset, TRI_sim_collection_t *collection) {
  TRI_join_part_t *part;
  TRI_join_feeder_t *feeder;
  
  part = (TRI_join_part_t *) TRI_Allocate(sizeof(TRI_join_part_t));
  if (part == 0) {
    return;
  }

  feeder = (TRI_join_feeder_t *) TRI_Allocate(sizeof(TRI_join_feeder_t));
  if (feeder == 0) {
    TRI_Free(part);
    return;
  }

  feeder->_collection = collection;
  feeder->init = InitFeeder;
  feeder->rewind = RewindFeeder;
  feeder->current = CurrentFeeder;
  feeder->next = NextFeeder;
  feeder->free = FreeFeeder;

  feeder->init(feeder);

  part->_type = type; 
  part->_resultset = resultset;
  part->_feeder = feeder; 
  part->free = FreePartJoin;
   
  TRI_PushBackVectorPointer(&join->_parts, part);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free join memory
////////////////////////////////////////////////////////////////////////////////

static void FreeJoin(TRI_join_t *join) {
  TRI_join_part_t *part;
  size_t n = join->_parts._length;

  if (n > 0) {
    while (true) {
      n--;
      part = join->_parts._buffer[n];
      part->free(part);
      if (n == 0) {
        break;
      }
    }
  }
  TRI_DestroyVectorPointer(&join->_parts);
  TRI_Free(join);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a join
////////////////////////////////////////////////////////////////////////////////

TRI_join_t *TRI_CreateJoin() {
  TRI_join_t *join = (TRI_join_t*) TRI_Allocate(sizeof(TRI_join_t));

  if (join == 0) {
    return 0;
  }

  TRI_InitVectorPointer(&join->_parts);

  join->free = FreeJoin;

  return join;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute joins
////////////////////////////////////////////////////////////////////////////////

void TRI_ExecuteJoin(TRI_join_t *join) {
  size_t numJoins;
  size_t current;
  TRI_join_feeder_t *feeder;
  TRI_doc_mptr_t *document;
  size_t numEval = 0;

  numJoins = join->_parts._length - 1;
  current  = numJoins;

  // actual join loop - implemented as nested loop join
  // it currently does not use indexes etc. so the performance is as worst as can be
  
  feeder = ((TRI_join_part_t *) join->_parts._buffer[current])->_feeder;
JOIN_NEXT:
  document = feeder->current(feeder);
  if (document) {
    numEval++;
    goto JOIN_NEXT;
  } 

JOIN_REPEAT:
  if (current == 0) {
    goto JOIN_END;
  }
  feeder->rewind(feeder);
  --current;
  feeder = ((TRI_join_part_t *) join->_parts._buffer[current])->_feeder;
  document = feeder->next(feeder);
  if (document) {
    current = numJoins;
    feeder = ((TRI_join_part_t *) join->_parts._buffer[current])->_feeder;
    goto JOIN_NEXT;
  }
  goto JOIN_REPEAT;

  assert(false);

JOIN_END:
  printf("JOIN NUM EVAL: %lu\n",numEval);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


