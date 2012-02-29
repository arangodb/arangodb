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
/// @brief Determine which geo indexes to use in a query
////////////////////////////////////////////////////////////////////////////////

static TRI_data_feeder_t* DetermineGeoIndexUsage (const TRI_vocbase_t* vocbase,
                                                  const TRI_select_join_t* join,
                                                  const size_t level,
                                                  const TRI_join_part_t* part) {
  TRI_vector_pointer_t* indexDefinitions;
  TRI_index_definition_t* indexDefinition;
  TRI_data_feeder_t* feeder = NULL;
  size_t i;
 
  assert(part->_geoRestriction);

  indexDefinitions = TRI_GetCollectionIndexes(vocbase, part->_collectionName);
  if (!indexDefinitions) {
    return feeder;
  }
    
  // enum all indexes
  for (i = 0; i < indexDefinitions->_length; i++) {
    indexDefinition = (TRI_index_definition_t*) indexDefinitions->_buffer[i];

    if (indexDefinition->_type != TRI_IDX_TYPE_GEO_INDEX) {
      // ignore all indexes except geo indexes here
      continue;
    }

    if (indexDefinition->_fields->_length != 2) {
      continue;
    }

    if (strcmp(indexDefinition->_fields->_buffer[0],
               part->_geoRestriction->_compareLat._field) != 0) {
      continue;
    }
    
    if (strcmp(indexDefinition->_fields->_buffer[1],
               part->_geoRestriction->_compareLon._field) != 0) {
      continue;
    }

    feeder = 
      TRI_CreateDataFeederGeoLookup((TRI_doc_collection_t*) part->_collection, 
                                    (TRI_join_t*) join, 
                                    level,
                                    part->_geoRestriction);

    if (feeder) {
      // set up addtl data for feeder
      feeder->_indexId = indexDefinition->_iid;
      break;
    }
  }

  TRI_FreeIndexDefinitions(indexDefinitions);

  return feeder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Determine which indexes to use in a query
////////////////////////////////////////////////////////////////////////////////

static TRI_data_feeder_t* DetermineIndexUsage (const TRI_vocbase_t* vocbase,
                                               const TRI_select_join_t* join,
                                               const size_t level,
                                               const TRI_join_part_t* part) {
  TRI_vector_pointer_t* indexDefinitions;
  TRI_index_definition_t* indexDefinition;
  TRI_data_feeder_t* feeder = NULL;
  QL_optimize_range_t* range;
  TRI_vector_pointer_t matches;
  size_t i, j, k;
  size_t numFieldsDefined;
  size_t numFields;
  size_t numConsts;
  size_t indexLength = 0;

  assert(!part->_geoRestriction);

  if (part->_ranges) {
    TRI_InitVectorPointer(&matches);
    indexDefinitions = TRI_GetCollectionIndexes(vocbase, part->_collectionName);
    if (!indexDefinitions) {
      goto EXIT2;
    }
    
    // enum all indexes
    for (i = 0; i < indexDefinitions->_length; i++) {
      indexDefinition = (TRI_index_definition_t*) indexDefinitions->_buffer[i];

      if (indexDefinition->_type == TRI_IDX_TYPE_GEO_INDEX) {
        // ignore all geo indexes here
        continue;
      }

      // reset compare state
      if (matches._length) {
        TRI_DestroyVectorPointer(&matches);
        TRI_InitVectorPointer(&matches);
      }
      numFields = 0;
      numConsts = 0;

      numFieldsDefined = indexDefinition->_fields->_length;
      for (j = 0 ; j < numFieldsDefined; j++) {
        // enumerate all fields from the index definition and 
        // check all ranges we found for the collection

        for (k = 0; k < part->_ranges->_length; k++) {
          range = (QL_optimize_range_t*) part->_ranges->_buffer[k];
          // check if collection name matches
          if (strcmp(range->_collection, part->_alias) != 0) {
            continue;
          }

          // check if field names match
          if (strcmp(indexDefinition->_fields->_buffer[j], 
                     range->_field) != 0) {
            continue;
          }

          if (indexDefinition->_type == TRI_IDX_TYPE_PRIMARY_INDEX ||
              indexDefinition->_type == TRI_IDX_TYPE_HASH_INDEX) {
            // check if index can be used
            // (primary and hash index only support equality comparisons)
            if (range->_minStatus == RANGE_VALUE_INFINITE || 
                range->_maxStatus == RANGE_VALUE_INFINITE) {
              // if this is an unbounded range comparison, continue
              continue;
            }
            if (range->_valueType == RANGE_TYPE_DOUBLE && 
                range->_minValue._doubleValue != range->_maxValue._doubleValue) {
              // if min double value != max value, continue
              continue; 
            }
            if ((range->_valueType == RANGE_TYPE_STRING || 
                 range->_valueType == RANGE_TYPE_JSON) && 
                strcmp(range->_minValue._stringValue, 
                       range->_maxValue._stringValue) != 0) {
              // if min string value != max value, continue
              continue; 
            }
          }

          if ((range->_valueType == RANGE_TYPE_FIELD && numConsts > 0) ||
              (range->_valueType != RANGE_TYPE_FIELD && numFields > 0)) {
            // cannot mix ref access and const access
            continue;
          }

          if (range->_valueType == RANGE_TYPE_FIELD) {
            // we found a reference
            numFields++;
          }
          else {
            // we found a constant
            numConsts++;
          }

          // push this candidate onto the stack
          TRI_PushBackVectorPointer(&matches, range);
          break;
        }
      }

      if (matches._length == numFieldsDefined) {
        // we have found as many matches as defined in the index definition
        // that means the index is fully covered in the condition

        if (indexDefinition->_type == TRI_IDX_TYPE_PRIMARY_INDEX) {
          // use the collection's primary index
          if (feeder) {
            // free any other feeder previously set up
            feeder->free(feeder);
          }

          feeder = 
            TRI_CreateDataFeederPrimaryLookup((TRI_doc_collection_t*) part->_collection, 
                                              (TRI_join_t*) join, 
                                              level);
          if (feeder) {
            // we always exit if we can use the primary index
            // the primary index guarantees uniqueness
            feeder->_ranges = TRI_CopyVectorPointer(&matches);
            goto EXIT;
          }
        } 

        if (indexLength < numFieldsDefined) {
          // if the index found contains more fields than the one we previously found, 
          // we use the new one 
          // (assumption: the more fields index, the less selective is the index)
          if (indexDefinition->_type == TRI_IDX_TYPE_HASH_INDEX ||
              indexDefinition->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
            // use a hash index defined for the collection
            if (feeder) {
              // free any other feeder previously set up
              feeder->free(feeder);
            }

            if (indexDefinition->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
              feeder = 
                TRI_CreateDataFeederSkiplistLookup((TRI_doc_collection_t*) part->_collection, 
                                                   (TRI_join_t*) join, 
                                                   level);
            }
            else {
              feeder = 
                TRI_CreateDataFeederHashLookup((TRI_doc_collection_t*) part->_collection, 
                                               (TRI_join_t*) join, 
                                               level);
            }

            if (feeder) {
              // set up addtl data for feeder
              feeder->_indexId = indexDefinition->_iid;
              feeder->_ranges = TRI_CopyVectorPointer(&matches);

              // for further comparisons
              indexLength = numFieldsDefined;
            }
          }
        }

      } 
    }

EXIT:
    TRI_FreeIndexDefinitions(indexDefinitions);
    TRI_DestroyVectorPointer(&matches);
  }

EXIT2:

  if (!feeder) {
    // if no index can be used, we'll do a full table scan
    feeder = TRI_CreateDataFeederTableScan((TRI_doc_collection_t*) part->_collection, 
                                           (TRI_join_t*) join, 
                                           level);
  }

  return feeder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return document data part type for a join part
////////////////////////////////////////////////////////////////////////////////

static inline TRI_select_part_e GetDocumentDataPartType(const TRI_join_type_e type) {
  if (type == JOIN_TYPE_LIST) {
    return RESULT_PART_DOCUMENT_MULTI;
  }
  return RESULT_PART_DOCUMENT_SINGLE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return value data part type for a join part
////////////////////////////////////////////////////////////////////////////////

static inline TRI_select_part_e GetValueDataPartType(const TRI_join_type_e type) {
  if (type == JOIN_TYPE_LIST) {
    return RESULT_PART_VALUE_MULTI;
  }
  return RESULT_PART_VALUE_SINGLE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select result from a join definition
////////////////////////////////////////////////////////////////////////////////

TRI_select_result_t* TRI_JoinSelectResult (const TRI_vocbase_t* vocbase,
                                           TRI_select_join_t* join) {
  TRI_select_result_t* result;
  TRI_vector_pointer_t* dataparts;
  TRI_select_datapart_t* datapart;
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
    if (part) {
      datapart = TRI_CreateDataPart(part->_alias, 
                                    part->_collection, 
                                    GetDocumentDataPartType(part->_type),
                                    0);
      if (datapart) {
        TRI_PushBackVectorPointer(dataparts, datapart);

        // if result contains some artificial extra data, create an additional
        // result part for it
        if (part->_extraData._size) {
          datapart = TRI_CreateDataPart(part->_extraData._alias,
                                        NULL,
                                        GetValueDataPartType(part->_type),
                                        part->_extraData._size);

          if (datapart) {
            TRI_PushBackVectorPointer(dataparts, datapart);
          }
          else {
            error = true;
          }
        }
      } 
      else {
        error = true;
      }

      // determine the access type (index usage/full table scan) for collection
      if (part->_geoRestriction) {
        part->_feeder = DetermineGeoIndexUsage(vocbase, join, i, part);
      }
      else {
        part->_feeder = DetermineIndexUsage(vocbase, join, i, part);
      }
  
      if (!part->_feeder) {
        error = true;
      }
    } 
    else {
      error = true;
    }
  }

  // set up the data structures to retrieve the result documents
  result = TRI_CreateSelectResult(dataparts);
  if (!result) {
    error = true;
  }

  if (error) {
    // clean up
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
    TRI_ClearVectorPointer(&part->_listDocuments);
    TRI_ClearVectorPointer(&part->_extraData._listValues);

    while (true) {
      // get next document
      if (!feeder->current(feeder)) {
        // end of documents in collection
        // exit this join
        break;
      }

      if (part->_condition && part->_context) {
        // check ON clause
        if (!CheckJoinClause(join, level)) {
          continue;
        }
      }

      // push documents into vector
      TRI_PushBackVectorPointer(&part->_listDocuments, part->_singleDocument);
      TRI_PushBackVectorPointer(&part->_extraData._listValues, part->_extraData._singleValue);
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
    if (!feeder->current(feeder)) {
      // end of documents in collection
      // exit this join
      break;
    }

    if (level > 0 && part->_condition && part->_context) {
      // check ON clause
      if (!CheckJoinClause(join, level)) {
        if (part->_type == JOIN_TYPE_OUTER) {
          // set document to null in outer join
          part->_singleDocument = NULL;
          part->_extraData._singleValue = NULL;

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

  for (i = 0; i < join->_parts._length; i++) {
    part = (TRI_join_part_t*) join->_parts._buffer[i];
    assert(part->_feeder);

    part->_feeder->init(part->_feeder);
  }
  // execute the join
  RecursiveJoin(results, join, 0, where, context, &_skip, &_limit);
  
  // deallocate vectors for list joins (would be done later anyway, but we can
  // free the memory here already)
  for (i = 0; i < join->_parts._length; i++) {
    part = (TRI_join_part_t*) join->_parts._buffer[i];
    if (part->_type == JOIN_TYPE_LIST) {
      TRI_DestroyVectorPointer(&part->_listDocuments);
      part->_listDocuments._buffer = NULL;

      TRI_DestroyVectorPointer(&part->_extraData._listValues);
      part->_extraData._listValues._buffer = NULL;
    }

    if (part->_feeder) {
      // free data feeder early, we don't need it any longer
      part->_feeder->free(part->_feeder);
      part->_feeder = NULL;
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

