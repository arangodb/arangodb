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

#include "query-join-execute.h"

#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "VocBase/query-join.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief log information about the used index
////////////////////////////////////////////////////////////////////////////////

static void LogIndexString(TRI_index_t const* idx,
                           char const* collectionName) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  size_t i;

  if (buffer == NULL) {
    return;
  }
  
  for (i = 0; i < idx->_fields._length; i++) {
    if (i > 0) {
      TRI_AppendStringStringBuffer(buffer, ", "); 
    }

    TRI_AppendStringStringBuffer(buffer, idx->_fields._buffer[i]);
  }

  LOG_DEBUG("using %s index (%s) for '%s'", 
            TRI_TypeNameIndex(idx), 
            buffer->_buffer, 
            collectionName);

  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief determines which geo indexes to use in a query
////////////////////////////////////////////////////////////////////////////////

static TRI_data_feeder_t* DetermineGeoIndexUsage (TRI_query_instance_t* const instance,
                                                  const size_t level,
                                                  const TRI_join_part_t* part) {
  TRI_vector_pointer_t* indexes;
  TRI_data_feeder_t* feeder = NULL;
  size_t i;
 
  assert(part->_geoRestriction);

  if (part->_collection->_collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_RegisterErrorQueryInstance(instance, 
                                   TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE,
                                   part->_alias);
    return NULL;
  }

  indexes = & ((TRI_sim_collection_t*) part->_collection->_collection)->_indexes;

  // enum all indexes
  for (i = 0; i < indexes->_length; i++) {
    TRI_index_t* idx;

    idx = (TRI_index_t*) indexes->_buffer[i];

    if (idx->_type != TRI_IDX_TYPE_GEO1_INDEX && idx->_type != TRI_IDX_TYPE_GEO2_INDEX) {
      // ignore all indexes except geo indexes here
      continue;
    }

    if (idx->_fields._length != 2) {
      continue;
    }

    if (! TRI_EqualString(idx->_fields._buffer[0], part->_geoRestriction->_compareLat._field)) {
      continue;
    }
    
    if (! TRI_EqualString(idx->_fields._buffer[1], part->_geoRestriction->_compareLon._field)) {
      continue;
    }

    feeder = 
      TRI_CreateDataFeederGeoLookup(instance,
                                    (TRI_doc_collection_t*) part->_collection->_collection, 
                                    level,
                                    idx->_iid,
                                    part->_geoRestriction);

    LogIndexString(idx, part->_alias);
    break;
  }

  if (feeder == NULL) {
    TRI_RegisterErrorQueryInstance(instance, 
                                   TRI_ERROR_QUERY_GEO_INDEX_MISSING, 
                                   part->_alias);
  }

  return feeder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Determine which indexes to use in a query
////////////////////////////////////////////////////////////////////////////////

static TRI_data_feeder_t* DetermineIndexUsage (TRI_query_instance_t* const instance,
                                               const size_t level,
                                               const TRI_join_part_t* part) {
  TRI_vector_pointer_t* indexes;
  TRI_data_feeder_t* feeder = NULL;
  size_t i;

  assert(part);
  assert(part->_alias);
  assert(!part->_geoRestriction);

  if (part->_ranges) {
    TRI_vector_pointer_t matches;
    size_t indexLength = 0;
    size_t numFieldsDefined;
    size_t numRefs;
    size_t numConsts;

    TRI_InitVectorPointer(&matches, TRI_UNKNOWN_MEM_ZONE);

    if (part->_collection->_collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
      TRI_RegisterErrorQueryInstance(instance, 
                                     TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE,
                                     part->_alias);
      return NULL;
    }

    indexes = & ((TRI_sim_collection_t*) part->_collection->_collection)->_indexes;

    // enum all indexes
    for (i = 0;  i < indexes->_length;  i++) {
      TRI_index_t* idx = (TRI_index_t*) indexes->_buffer[i];
      QL_optimize_range_compare_type_e lastCompareType = COMPARE_TYPE_UNKNOWN;
      size_t j;

      if (idx->_type == TRI_IDX_TYPE_GEO2_INDEX || idx->_type == TRI_IDX_TYPE_GEO2_INDEX) {
        // ignore all geo indexes here
        continue;
      }
      
      numFieldsDefined = idx->_fields._length;

      // reset compare state
      numRefs = 0;
      numConsts = 0;
      if (matches._length) {
        TRI_DestroyVectorPointer(&matches);
        TRI_InitVectorPointer(&matches, TRI_UNKNOWN_MEM_ZONE);
      }

      for (j = 0;  j < numFieldsDefined;  j++) {
        size_t k;

        // enumerate all fields from the index definition and 
        // check all ranges we found for the collection
        for (k = 0; k < part->_ranges->_length; k++) {
          QL_optimize_range_t* range = (QL_optimize_range_t*) part->_ranges->_buffer[k];

          if (!range) {
            continue;
          }

          assert(range->_collection);

          // check if collection name matches
          if (! TRI_EqualString(range->_collection, part->_alias)) {
            continue;
          }

          // check if field names match
          if (! TRI_EqualString(idx->_fields._buffer[j], range->_field)) {
            continue;
          }
          
          // check whether we'll be mixing const and ref access
          if ((range->_valueType == RANGE_TYPE_REF && numConsts > 0) ||
              (range->_valueType != RANGE_TYPE_REF && numRefs > 0)) {
            // cannot mix ref access and const access
            continue;
          }
          
          // primary and hash index only support equality comparisons
          if ((idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX ||
               idx->_type == TRI_IDX_TYPE_HASH_INDEX) && 
              !QLIsEqualRange(range)) {
            // this is not an equality comparison, continue
            continue;
          }

          if (idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
            QL_optimize_range_compare_type_e compareType = QLGetCompareTypeRange(range);

            if (lastCompareType == COMPARE_TYPE_UNKNOWN) {
              if (compareType != COMPARE_TYPE_EQ && 
                  compareType != COMPARE_TYPE_LT &&
                  compareType != COMPARE_TYPE_GT &&
                  compareType != COMPARE_TYPE_LE &&
                  compareType != COMPARE_TYPE_GE &&
                  compareType != COMPARE_TYPE_BETWEEN) {
                // unsupported initial compare type for skiplist index
                continue;
              }
            }
            else if (lastCompareType == COMPARE_TYPE_EQ) {
              if (compareType != COMPARE_TYPE_EQ && 
                  compareType != COMPARE_TYPE_LT &&
                  compareType != COMPARE_TYPE_GT &&
                  compareType != COMPARE_TYPE_LE &&
                  compareType != COMPARE_TYPE_GE &&
                  compareType != COMPARE_TYPE_BETWEEN) {
                // unsupported continued compare type for skiplist index
                continue;
              }
            }
            else {
              // unsupported continued compare type for skiplist index
              continue;
            }

            lastCompareType = compareType;
          }

          if (range->_valueType == RANGE_TYPE_REF) {
            // we found a reference
            numRefs++;
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

        if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX) {
          // use the collection's primary index
          if (feeder) {
            // free any other feeder previously set up
            feeder->free(feeder);
          }

          feeder = 
            TRI_CreateDataFeederPrimaryLookup(instance, 
                                              (TRI_doc_collection_t*) part->_collection->_collection, 
                                              level,
                                              TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &matches));
          if (feeder) {
            // we always exit if we can use the primary index
            // the primary index guarantees uniqueness
            LogIndexString(idx, part->_alias);
            goto EXIT;
          }
        } 

        if (indexLength < numFieldsDefined) {
          // if the index found contains more fields than the one we previously found, 
          // we use the new one 
          // (assumption: the more fields index, the less selective is the index)
          if (idx->_type == TRI_IDX_TYPE_HASH_INDEX ||
              idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
            // use a hash index defined for the collection
            if (feeder) {
              // free any other feeder previously set up
              feeder->free(feeder);
            }

            if (idx->_type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
              feeder = 
                TRI_CreateDataFeederSkiplistLookup(instance, 
                                                   (TRI_doc_collection_t*) part->_collection->_collection, 
                                                   level,
                                                   idx->_iid,
                                                   TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &matches));
            
              LogIndexString(idx, part->_alias);
            }
            else {
              feeder = 
                TRI_CreateDataFeederHashLookup(instance, 
                                               (TRI_doc_collection_t*) part->_collection->_collection, 
                                               level,
                                               idx->_iid,
                                               TRI_CopyVectorPointer(TRI_UNKNOWN_MEM_ZONE, &matches));
              
              LogIndexString(idx, part->_alias);
            }

            if (!feeder) {
              TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
              goto EXIT;
            }

            // for further comparisons
            indexLength = numFieldsDefined;
          }
        }

      } 
    }

EXIT:
    TRI_DestroyVectorPointer(&matches);
  }

  if (!feeder) {
    // if no index can be used, we'll do a full table scan
    feeder = TRI_CreateDataFeederTableScan(instance,
                                           (TRI_doc_collection_t*) part->_collection->_collection, 
                                           level);
    
    LOG_DEBUG("using table scan for '%s'", part->_alias);
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
/// @brief Free join data parts
////////////////////////////////////////////////////////////////////////////////

static void FreeDataParts (TRI_query_instance_t* const instance,
                           TRI_vector_pointer_t* const dataparts) {
  size_t i;

  assert(instance);
  assert(dataparts);
    
  for (i = 0; i < dataparts->_length; i++) {
    TRI_select_datapart_t* datapart = (TRI_select_datapart_t*) dataparts->_buffer[i];
    if (datapart) {
      datapart->free(datapart);
    }
  }

  TRI_DestroyVectorPointer(dataparts);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, dataparts);
  
  for (i = 0; i < instance->_join._length; i++) {
    TRI_join_part_t* part = (TRI_join_part_t*) instance->_join._buffer[i];
    if (part->_feeder) {
      part->_feeder->free(part->_feeder);
      part->_feeder = NULL;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a datapart for a definition
////////////////////////////////////////////////////////////////////////////////

static bool CreateCollectionDataPart (TRI_query_instance_t* const instance,
                                      TRI_vector_pointer_t* const dataparts,
                                      const TRI_join_part_t* const part) {
  TRI_select_datapart_t* datapart =
    TRI_CreateDataPart(part->_alias, 
                       part->_collection->_collection, 
                       GetDocumentDataPartType(part->_type),
                       0,
                       part->_mustMaterialize._select,
                       part->_mustMaterialize._order);
  if (!datapart) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }

  TRI_PushBackVectorPointer(dataparts, datapart);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a geo data part
////////////////////////////////////////////////////////////////////////////////

static bool CreateGeoDataPart (TRI_query_instance_t* const instance,
                               TRI_vector_pointer_t* const dataparts,
                               const TRI_join_part_t* const part) {
  TRI_select_datapart_t* datapart =
    TRI_CreateDataPart(part->_extraData._alias,
                       NULL,
                       GetValueDataPartType(part->_type),
                       part->_extraData._size,
                       part->_mustMaterialize._select,
                       part->_mustMaterialize._order);

  if (!datapart) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }

  TRI_PushBackVectorPointer(dataparts, datapart);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create dataparts for a join definition
////////////////////////////////////////////////////////////////////////////////

static bool CreateDataParts (TRI_query_instance_t* const instance,
                             TRI_vector_pointer_t* const dataparts) {
  size_t i;

  for (i = 0; i < instance->_join._length; i++) {
    TRI_join_part_t* part = (TRI_join_part_t*) instance->_join._buffer[i];

    assert(part);

    if (part->_mustMaterialize._select || part->_mustMaterialize._order) {
      if (!CreateCollectionDataPart(instance, dataparts, part)) {
        return false;
      }
    }

    // if result contains some artificial extra data, create an additional
    // result part for it
    if (part->_extraData._size) {
      if (!CreateGeoDataPart(instance, dataparts, part)) {
        return false;
      }
    }

    // determine the access type (index usage/full table scan) for collection
    if (part->_geoRestriction) {
      part->_feeder = DetermineGeoIndexUsage(instance, i, part);
    }
    else {
      part->_feeder = DetermineIndexUsage(instance, i, part);
    }

    if (!part->_feeder) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new select result from a join definition
////////////////////////////////////////////////////////////////////////////////

TRI_select_result_t* TRI_JoinSelectResult (TRI_query_instance_t* const instance) {
  TRI_select_result_t* result;
  TRI_vector_pointer_t* dataparts;

  dataparts = (TRI_vector_pointer_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_pointer_t), false);
  if (!dataparts) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  TRI_InitVectorPointer(dataparts, TRI_UNKNOWN_MEM_ZONE);
  if (!CreateDataParts(instance, dataparts)) {
    // clean up
    FreeDataParts(instance, dataparts);
    return NULL;
  }

  // set up the data structures to retrieve the result documents
  result = TRI_CreateSelectResult(dataparts);
  if (!result) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    // clean up
    FreeDataParts(instance, dataparts);
    return NULL;
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Check an ON condition for a join
////////////////////////////////////////////////////////////////////////////////

static inline bool CheckJoinClause (TRI_query_instance_t* const instance,
                                    const size_t level) {
  TRI_join_part_t* part;
  bool whereResult = false;

  part = (TRI_join_part_t*) instance->_join._buffer[level];
  TRI_DefineWhereExecutionContext(instance, part->_context, level, true);
  if (!TRI_ExecuteConditionExecutionContext(instance, part->_context, &whereResult)) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_QUERY_RUNTIME_ERROR, "");
    return false;
  }

  return whereResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Check a WHERE condition
////////////////////////////////////////////////////////////////////////////////

static inline bool CheckWhereClause (TRI_query_instance_t* const instance,
                                     const size_t level) {
  bool whereResult;
  
  TRI_DefineWhereExecutionContext(instance, instance->_query._where._context, level, false);
  if (!TRI_ExecuteConditionExecutionContext(instance, instance->_query._where._context, &whereResult)) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_QUERY_RUNTIME_ERROR, "");
    return false;
  }

  return whereResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Forward declaration
////////////////////////////////////////////////////////////////////////////////

static bool ProcessRow (TRI_query_instance_t* const,
                        TRI_select_result_t*,
                        const size_t,
                        TRI_voc_size_t*,
                        TRI_voc_ssize_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute join part recursively
////////////////////////////////////////////////////////////////////////////////

static bool RecursiveJoin (TRI_query_instance_t* const instance,
                           TRI_select_result_t* results,
                           const size_t level,
                           TRI_voc_size_t* skip,
                           TRI_voc_ssize_t* limit) {
  TRI_join_part_t* part;
  TRI_data_feeder_t* feeder;

  if (*limit == 0) {
    return false;
  }

  part = (TRI_join_part_t*) instance->_join._buffer[level];
  feeder = part->_feeder;
  feeder->rewind(feeder);

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

      if (part->_context) {
        // check ON clause
        if (!CheckJoinClause(instance, level)) {
          continue;
        }
      }

      // push documents into vector
      TRI_PushBackVectorPointer(&part->_listDocuments, part->_singleDocument);
      TRI_PushBackVectorPointer(&part->_extraData._listValues, part->_extraData._singleValue);
    }

    return ProcessRow(instance, results, level, skip, limit);
    // end of list join
  }
  else {
    // join type is non-aggregate
    bool joinMatch = false;

    while (true) {
      // get next document
      if (!feeder->current(feeder)) {
        // end of documents in collection
        // exit this join

        if (part->_type == JOIN_TYPE_OUTER && !joinMatch) {
          // left join: if we are not at the last document of the left
          // joined collection, we continue
          // if we are at the last document and there hasn't been a 
          // match, we must still include the left document if the where
          // condition matches
          if (!ProcessRow(instance, results, level, skip, limit)) {
            return false;
          }
        }
        break;
      }

      if (level > 0 && part->_context) {
        // check ON clause
        if (!CheckJoinClause(instance, level)) {
          if (part->_type == JOIN_TYPE_OUTER) {
            // set document to null in outer join
            part->_singleDocument = NULL;
            part->_extraData._singleValue = NULL;
          }
          continue;
        }
        joinMatch = true;
      }

      if (!ProcessRow(instance, results, level, skip, limit)) {
        return false;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Process a single result row in a join
////////////////////////////////////////////////////////////////////////////////

static bool ProcessRow (TRI_query_instance_t* const instance,
                        TRI_select_result_t* results,
                        const size_t level,
                        TRI_voc_size_t* skip,
                        TRI_voc_ssize_t* limit) {
  
  size_t numJoins = instance->_join._length - 1;

  if (level == numJoins) {
    // last join was executed
    if (instance->_query._where._type == QLQueryWhereTypeMustEvaluate) {
      // check WHERE clause for full row
      if (!CheckWhereClause(instance, level)) {
        return true;
      }
    }

    // apply SKIP limits
    if (*skip > 0) {
      (*skip)--;
      return true;
    }
      
    // full match, now add documents to result sets
    TRI_AddJoinSelectResult(instance, results); 

    (*limit)--;
    // apply LIMIT
    if (*limit == 0) {
      return false;
    }

    return true; 
  }

  // recurse into next join
  return RecursiveJoin(instance, results, level + 1, skip, limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init join data structures
////////////////////////////////////////////////////////////////////////////////

static void InitJoin (TRI_query_instance_t* const instance) {
  size_t i;

  for (i = 0; i < instance->_join._length; i++) {
    TRI_join_part_t* part = (TRI_join_part_t*) instance->_join._buffer[i];
    assert(part->_feeder);

    part->_feeder->init(part->_feeder);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief De-init join data structures
////////////////////////////////////////////////////////////////////////////////

static void DeinitJoin (TRI_query_instance_t* const instance) {
  // deallocate vectors for list joins (would be done later anyway, but we can
  // free the memory here already)
  size_t i;

  for (i = 0; i < instance->_join._length; i++) {
    TRI_join_part_t* part = (TRI_join_part_t*) instance->_join._buffer[i];
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
/// @brief Execute joins
////////////////////////////////////////////////////////////////////////////////

void TRI_ExecuteJoins (TRI_query_instance_t* const instance,
                       TRI_select_result_t* results,
                       const TRI_voc_size_t skip,
                       const TRI_voc_ssize_t limit) {
  // set skip and limit to initial values
  // (values will be decreased during join execution)
  TRI_voc_size_t _skip = skip;
  TRI_voc_ssize_t _limit = limit;
  bool result;

  assert(instance);
  LOG_DEBUG("executing %i-way join", (int) instance->_join._length);
  LOG_DEBUG("skip: %li, limit: %li", (long int) skip, (long int) limit);

  // init data parts
  InitJoin(instance);
  
  // execute the join
  result = RecursiveJoin(instance, results, 0, &_skip, &_limit);
  if (!result) {
    LOG_DEBUG("limit reached");
  }

  // de-init data parts
  DeinitJoin(instance);
  
  LOG_DEBUG("join finished");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
