////////////////////////////////////////////////////////////////////////////////
/// @brief join executor
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

#include "join-execute.h"
#include "join.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Return data part type for a join part
////////////////////////////////////////////////////////////////////////////////

static inline TRI_select_part_e JoinPartDataPartType(const TRI_join_type_e type) {
  if (type == JOIN_TYPE_LIST) {
    return RESULT_PART_MULTI;
  }
  return RESULT_PART_SINGLE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select result from a join definition
////////////////////////////////////////////////////////////////////////////////

TRI_select_result_t* TRI_JoinSelectResult (TRI_select_join_t* join) {
  TRI_select_result_t* result;
  TRI_vector_pointer_t* dataparts;
  TRI_select_datapart_t* datapart;
  TRI_data_feeder_t* feeder;
  TRI_join_part_t* part;
  size_t i;
  bool error = false;

  dataparts = (TRI_vector_pointer_t*) TRI_Allocate(sizeof(TRI_vector_pointer_t));
  if (!dataparts) {
    return NULL;
  }

  TRI_InitVectorPointer(dataparts);
  for (i = 0; i < join->_parts._length; i++) {
    part = (TRI_join_part_t*) join->_parts._buffer[i];
    datapart = TRI_CreateDataPart(part->_alias, 
                                  part->_collection, 
                                  JoinPartDataPartType(part->_type));
    if (datapart) {
      TRI_PushBackVectorPointer(dataparts, datapart);
    } 
    else {
      error = true;
    }
  
    feeder = TRI_CreateDataFeeder((TRI_doc_collection_t*) part->_collection);
    if (feeder) {
      part->_feeder = feeder;
    } 
    else {
      error = true;
    }
  }

  result = TRI_CreateSelectResult(dataparts);
  if (!result) {
    error = true;
  }

  if (error) {
    for (i = 0; i < dataparts->_length; i++) {
      datapart = (TRI_select_datapart_t*) dataparts->_buffer[i];
      if (datapart) {
        datapart->free(datapart);
      }
    }

    for (i = 0; i < join->_parts._length; i++) {
      part = (TRI_join_part_t*) join->_parts._buffer[i];
      if (part->_feeder) {
        part->_feeder->free(part->_feeder);
        part->_feeder = NULL;
      }
    }

    TRI_DestroyVectorPointer(dataparts);
    TRI_Free(dataparts);
    return NULL;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Check an ON condition for a join
////////////////////////////////////////////////////////////////////////////////

static inline bool CheckJoinClause (const TRI_select_join_t const* join, 
                                    const size_t level) {
  TRI_join_part_t* part;
  bool whereResult;

  part = (TRI_join_part_t*) join->_parts._buffer[level];
  TRI_DefineWhereExecutionContext(part->_context, join, level, true);
  TRI_ExecuteConditionExecutionContext(part->_context, &whereResult);

  return whereResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Check a WHERE condition
////////////////////////////////////////////////////////////////////////////////

static inline bool CheckWhereClause (const TRI_rc_context_t *context, 
                                     const TRI_select_join_t const* join, 
                                     const size_t level) {
  bool whereResult;
  
  TRI_DefineWhereExecutionContext(context->_whereClause, join, level, false);
  TRI_ExecuteConditionExecutionContext(context->_whereClause, &whereResult);

  return whereResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute join part recursively
////////////////////////////////////////////////////////////////////////////////

static void RecursiveJoin (TRI_select_result_t* results,
                           TRI_select_join_t* join,
                           const size_t level,
                           TRI_qry_where_t *where,
                           TRI_rc_context_t *context,
                           TRI_voc_size_t *skip,
                           TRI_voc_ssize_t *limit) {
  TRI_join_part_t* part;
  TRI_data_feeder_t* feeder;
  TRI_doc_mptr_t* document;
  size_t numJoins;
  bool joinMatch = false;

  if (*limit == 0) {
    return;
  }

  numJoins = join->_parts._length - 1;
  assert(level <= numJoins);

  part = (TRI_join_part_t*) join->_parts._buffer[level];
  feeder = part->_feeder;
  feeder->rewind(feeder);


  // distinction between aggregates (list joins) and 
  // non-aggregates (inner join, left join)

  if (part->_type == JOIN_TYPE_LIST) {
    // join type is aggregate (list join)
    assert(level > 0);
    part->_singleDocument = NULL;
    TRI_ClearVectorPointer(&part->_listDocuments);

    while (true) {
      // get next document
      document = feeder->current(feeder);
      if (!document) {
        // end of documents in collection
        // exit this join
        break;
      }

      part->_singleDocument = document;
      if (part->_condition && part->_context) {
        // check ON clause
        if (!CheckJoinClause(join, level)) {
          continue;
        }
      }

      // push documents into vector
      TRI_PushBackVectorPointer(&part->_listDocuments, document);
    }
    
    // all documents collected in vector
    if (level == numJoins) {
      // last join was executed

      if (where != 0) {
        // check WHERE clause for full row
        if (!CheckWhereClause(context, join, level)) {
          return;
        }
      }

      // apply SKIP limits
      if (*skip > 0) {
        (*skip)--;
        return;
      }
      
      // full match, now add documents to result sets
      TRI_AddJoinSelectResult(results, join); 

      (*limit)--;
      // apply LIMIT
      if (*limit == 0) {
        return;
      }
    }
    else {
      // recurse into next join
      RecursiveJoin(results, join, level + 1, where, context, skip, limit);
    }

    return;
    // end of list join
  }

  // join type is non-aggregate

  while (true) {
    // get next document
    document = feeder->current(feeder);

    if (!document) {
      // end of documents in collection
      // exit this join
      break;
    }

    part->_singleDocument = document;
    if (level > 0 && part->_condition && part->_context) {
      // check ON clause
      if (!CheckJoinClause(join, level)) {
        if (part->_type == JOIN_TYPE_OUTER) {
          // set document to null in outer join
          part->_singleDocument = NULL;

          // left join: if we are not at the last document of the left
          // joined collection, we continue
          // if we are at the last document and there hasn't been a 
          // match, we must still include the left document if the where
          // condition matches
          if (joinMatch || !feeder->eof(feeder)) {
            continue;
          }
        }
        else {
          // inner join: always go to next record
          continue;
        }
      }
      joinMatch = true;
    } 

    if (level == numJoins) {
      // last join was executed

      if (where != 0) {
        // check WHERE clause for full row
        if (!CheckWhereClause(context, join, level)) {
          continue;
        }
      }

      // apply SKIP limits
      if (*skip > 0) {
        (*skip)--;
        continue;
      }
      
      // full match, now add documents to result sets
      TRI_AddJoinSelectResult(results, join); 

      (*limit)--;
      // apply LIMIT
      if (*limit == 0) {
        return;
      }
    }
    else {
      // recurse into next join
      RecursiveJoin(results, join, level + 1, where, context, skip, limit);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute joins
////////////////////////////////////////////////////////////////////////////////

void TRI_ExecuteJoins (TRI_select_result_t* results,
                       TRI_select_join_t* join, 
                       TRI_qry_where_t* where,
                       TRI_rc_context_t* context,
                       const TRI_voc_size_t skip,
                       const TRI_voc_ssize_t limit) {
  TRI_join_part_t* part;
  TRI_voc_size_t _skip;
  TRI_voc_ssize_t _limit;
  size_t i;

  // set skip and limit to initial values
  // (values will be decreased during join execution)
  _skip = skip;
  _limit = limit;
 
  // execute the join
  RecursiveJoin(results, join, 0, where, context, &_skip, &_limit);
  
  // deallocate vectors for list joins (would be done later anyway, but we can
  // free the memory here already)
  for (i = 0; i < join->_parts._length; i++) {
    part = (TRI_join_part_t*) join->_parts._buffer[i];
    if (part->_type == JOIN_TYPE_LIST) {
      TRI_DestroyVectorPointer(&part->_listDocuments);
      part->_listDocuments._buffer = NULL;
    }
    if (part->_feeder) {
      part->_feeder->free(part->_feeder);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

