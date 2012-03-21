////////////////////////////////////////////////////////////////////////////////
/// @brief Basic query data structures
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

#include <BasicsC/logging.h>

#include "VocBase/query-join.h"
#include "VocBase/query-base.h"
#include "VocBase/query-parse.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash bind parameter names
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashBindParameterName (TRI_associative_pointer_t* array, 
                                       void const* key) {
  return TRI_FnvHashString((char const*) key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash bind parameters
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashBindParameter (TRI_associative_pointer_t* array, 
                                   void const* element) {
  TRI_bind_parameter_t* parameter = (TRI_bind_parameter_t*) element;

  return TRI_FnvHashString(parameter->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Comparison function used to determine bind parameter equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualBindParameter (TRI_associative_pointer_t* array, 
                                void const* key, 
                                void const* element) {
  TRI_bind_parameter_t* parameter = (TRI_bind_parameter_t*) element;

  return TRI_EqualString(key, parameter->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free all bind parameters in associative array
////////////////////////////////////////////////////////////////////////////////

static void FreeBindParameters (TRI_associative_pointer_t* array) {
  TRI_bind_parameter_t* parameter;
  size_t i;

  for (i = 0; i < array->_nrAlloc; i++) {
    parameter = (TRI_bind_parameter_t*) array->_table[i];
    if (parameter) {
      TRI_FreeBindParameter(parameter);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the select part
////////////////////////////////////////////////////////////////////////////////

static bool InitSelectQueryTemplate (TRI_query_template_t* const template_) {
  QL_ast_query_select_t* select_;
  char* primaryAlias;

  select_ = &template_->_query->_select;
  QLOptimizeExpression(select_->_base);
  primaryAlias = QLAstQueryGetPrimaryAlias(template_->_query);
  select_->_type = QLOptimizeGetSelectType(select_->_base, primaryAlias);
  select_->_usesBindParameters = QLOptimizeUsesBindParameters(select_->_base);
  select_->_functionCode = NULL;

  if (select_->_type == QLQuerySelectTypeEvaluated && 
      !select_->_usesBindParameters) {
    // create JS code
    select_->_functionCode = TRI_GetFunctionCodeQueryJavascript(
      select_->_base, 
      &template_->_bindParameters
    );

    if (!select_->_functionCode) {
      TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }
    TRI_RegisterStringQuery(&template_->_memory._strings, select_->_functionCode);
  }

  LOG_DEBUG("created select part of query template. type: %i, "
            "usesBindParameters: %i functionCode: %s",
            (int) select_->_type,
            (int) select_->_usesBindParameters,
            (select_->_functionCode ? select_->_functionCode : ""));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the where part
////////////////////////////////////////////////////////////////////////////////

static bool InitWhereQueryTemplate (TRI_query_template_t* const template_) {
  QL_ast_query_where_t* where;

  where = &template_->_query->_where;
  QLOptimizeExpression(where->_base);
  where->_type = QLOptimizeGetWhereType(where->_base);
  where->_usesBindParameters = QLOptimizeUsesBindParameters(where->_base);
  where->_functionCode = NULL;

  if (where->_type == QLQueryWhereTypeMustEvaluate && 
      !where->_usesBindParameters) {
    // create JS code
    where->_functionCode = TRI_GetFunctionCodeQueryJavascript(
      where->_base, 
      &template_->_bindParameters
    );

    if (!where->_functionCode) {
      TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }
    TRI_RegisterStringQuery(&template_->_memory._strings, where->_functionCode);
  }
  
  LOG_DEBUG("created where part of query template. type: %i, "
            "usesBindParameters: %i, functionCode: %s",
            (int) where->_type,
            (int) where->_usesBindParameters,
            (where->_functionCode ? where->_functionCode : ""));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the order part
////////////////////////////////////////////////////////////////////////////////

static bool InitOrderQueryTemplate (TRI_query_template_t* const template_) {
  QL_ast_query_order_t* order;

  // optimize order clause
  order = &template_->_query->_order;
  order->_type = QLQueryOrderTypeNone;
  order->_usesBindParameters = false;
  order->_functionCode = NULL;

  if (order->_base) {
    QLOptimizeOrder(order->_base);
    order->_type = QLOptimizeGetOrderType(order->_base);
    order->_usesBindParameters = QLOptimizeUsesBindParameters(order->_base);
    if (order->_type == QLQueryOrderTypeMustEvaluate &&
        !order->_usesBindParameters) {
      // create JS code
      order->_functionCode = TRI_GetOrderFunctionCodeQueryJavascript(
        order->_base, 
        &template_->_bindParameters
      );

      if (!order->_functionCode) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return false;
      }
      TRI_RegisterStringQuery(&template_->_memory._strings, order->_functionCode);
    }
  }
  
  LOG_DEBUG("created order part of query template. type: %i, "
            "usesBindParameters: %i, functionCode: %s",
            (int) order->_type,
            (int) order->_usesBindParameters,
            (order->_functionCode ? order->_functionCode : ""));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the from part
////////////////////////////////////////////////////////////////////////////////

static bool InitFromQueryTemplate (TRI_query_template_t* const template_) {
  TRI_query_node_t* node;
  
  assert(template_);
  assert(template_->_query);
  
  node = template_->_query->_from._base->_next;

  // iterate over all joins
  while (node) {
    if (node->_rhs && 
        TRI_QueryNodeGetTypeGroup(node->_type) == TRI_QueryNodeGroupJoin) {
      QL_ast_query_collection_t* collection;
      char* alias;

      alias = node->_lhs->_rhs->_value._stringValue;
      assert(alias);

      collection = (QL_ast_query_collection_t*)
        TRI_LookupByKeyAssociativePointer(&template_->_query->_from._collections, alias);

      assert(collection);
      
      // optimize on clause
      QLOptimizeExpression(node->_rhs);
      
      collection->_where._base = node->_rhs;
      collection->_where._type = QLOptimizeGetWhereType(node->_rhs);
      collection->_where._usesBindParameters = QLOptimizeUsesBindParameters(node->_rhs);

      if (collection->_where._type == QLQueryWhereTypeMustEvaluate && 
          !collection->_where._usesBindParameters) {
        // create JS code
        collection->_where._functionCode = TRI_GetFunctionCodeQueryJavascript(
          node->_rhs,
          &template_->_bindParameters
        );
    
        if (!collection->_where._functionCode) {
          TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return false;
        }
        TRI_RegisterStringQuery(&template_->_memory._strings, collection->_where._functionCode);
      }
  
      LOG_DEBUG("created from part of query template. alias: %s, name: %s, "
                "type: %i, usesBindParameters: %i, functionCode: %s",
                collection->_alias,
                collection->_name,
                (int) collection->_where._type,
                (int) collection->_where._usesBindParameters,
                (collection->_where._functionCode ? collection->_where._functionCode : ""));
    }
    node = node->_next;
  }

  if (template_->_query->_from._collections._nrUsed > QUERY_MAX_JOINS) {
    TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_TOO_MANY_JOINS, NULL);
    return false;
  }

  QLOptimizeFrom(template_->_query);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   bind parameters
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the names of all bind parameters
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_GetNamesBindParameter (TRI_associative_pointer_t* const parameters) {
  TRI_vector_string_t names;
  size_t i;

  TRI_InitVectorString(&names);

  assert(parameters);
  // enumerate all bind parameters....
  for (i = 0; i < parameters->_nrAlloc; i++) {
    TRI_bind_parameter_t* parameter;
    char* copy;

    parameter = (TRI_bind_parameter_t*) parameters->_table[i];
    if (!parameter) {
      continue;
    }

    copy = TRI_DuplicateString(parameter->_name);
    if (copy) {
      TRI_PushBackVectorString(&names, copy);
    }
  }

  return names;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a single bind parameter
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBindParameter (TRI_bind_parameter_t* const parameter) {
  assert(parameter);
  
  if (parameter->_name) {
    TRI_Free(parameter->_name);
  }

  if (parameter->_data) {
    TRI_FreeJson(parameter->_data);
  }

  TRI_Free(parameter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a bind parameter
////////////////////////////////////////////////////////////////////////////////

TRI_bind_parameter_t* TRI_CreateBindParameter (const char* name, 
                                               const TRI_json_t* data) {
  TRI_bind_parameter_t* parameter;

  assert(name);

  parameter = (TRI_bind_parameter_t*) TRI_Allocate(sizeof(TRI_bind_parameter_t));
  if (!parameter) {
    return NULL;
  }

  parameter->_name = TRI_DuplicateString(name);
  if (!parameter->_name) {
    TRI_Free(parameter);
    return NULL;
  }

  parameter->_data = NULL;
  if (data) {
    parameter->_data = TRI_CopyJson((TRI_json_t*) data);
    if (!parameter->_data) {
      TRI_Free(parameter->_name);
      TRI_Free(parameter);
      return NULL;
    }
  }

  return parameter;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    query template
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Initialize the structs contained in a query template and perform
/// some basic optimizations and type detections
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitQueryTemplate (TRI_query_template_t* const template_) {
  return
    InitSelectQueryTemplate(template_) &&
    InitWhereQueryTemplate(template_) &&
    InitOrderQueryTemplate(template_) &&
    InitFromQueryTemplate(template_);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a bind parameter to a query template
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBindParameterQueryTemplate (TRI_query_template_t* const template_,
                                        const TRI_bind_parameter_t* const parameter) {
  assert(template_);

  if (!parameter) {
    TRI_SetQueryError(&template_->_error, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  assert(parameter->_name);
  
  // bind parameter redeclared
  if (TRI_LookupByKeyAssociativePointer(&template_->_bindParameters, 
                                        parameter->_name)) {
    TRI_FreeBindParameter((TRI_bind_parameter_t* const) parameter);
    return false;
  }

  TRI_InsertKeyAssociativePointer(&template_->_bindParameters, 
                                  parameter->_name,
                                  (void*) parameter, 
                                  true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a query template
////////////////////////////////////////////////////////////////////////////////

TRI_query_template_t* TRI_CreateQueryTemplate (const char* queryString, 
                                               const TRI_vocbase_t* const vocbase) {
  TRI_query_template_t* template_;
  
  assert(queryString);
  assert(vocbase);

  template_ = (TRI_query_template_t*) TRI_Allocate(sizeof(TRI_query_template_t));
  if (!template_) {
    return NULL;
  }
  
  template_->_queryString = TRI_DuplicateString(queryString);
  if (!template_->_queryString) {
    TRI_Free(template_);
    return NULL;
  }
  
  template_->_query = (QL_ast_query_t*) TRI_Allocate(sizeof(QL_ast_query_t));
  if (!template_->_query) {
    TRI_Free(template_->_queryString);
    TRI_Free(template_);
    return NULL;
  }

  template_->_vocbase = (TRI_vocbase_t*) vocbase;
  TRI_InitQueryError(&template_->_error);

  TRI_InitAssociativePointer(&template_->_bindParameters,
                             HashBindParameterName,
                             HashBindParameter,
                             EqualBindParameter,
                             0);
  
  // init vectors needed for book-keeping memory
  TRI_InitVectorPointer(&template_->_memory._nodes);
  TRI_InitVectorPointer(&template_->_memory._strings);
  TRI_InitVectorPointer(&template_->_memory._listHeads);
  TRI_InitVectorPointer(&template_->_memory._listTails);
  
  QLAstQueryInit(template_->_query);
  TRI_InitMutex(&template_->_lock);
  
  LOG_DEBUG("created query template for query %s", queryString);

  return template_;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a query template
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryTemplate (TRI_query_template_t* template_) {
  assert(template_);
  assert(template_->_queryString);
  assert(template_->_query);

  QLAstQueryFree(template_->_query);
  TRI_Free(template_->_query);
  
  // list elements in _listHeads and _listTails must not be freed separately as
  // they are AstNodes handled by _nodes already
  
  // free vectors themselves
  TRI_FreeNodeVectorQuery(&template_->_memory._nodes);
  TRI_FreeStringVectorQuery(&template_->_memory._strings);
  TRI_DestroyVectorPointer(&template_->_memory._listHeads);
  TRI_DestroyVectorPointer(&template_->_memory._listTails);

  TRI_Free(template_->_queryString);
 
  TRI_FreeQueryError(&template_->_error); 
  
  FreeBindParameters(&template_->_bindParameters);
  TRI_DestroyAssociativePointer(&template_->_bindParameters);
  TRI_DestroyMutex(&template_->_lock);

  TRI_Free(template_);
  
  LOG_DEBUG("destroyed query template");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    query instance
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get size of extra data in geo join parts 
////////////////////////////////////////////////////////////////////////////////

static size_t GetJoinPartExtraDataSizeGeo (TRI_join_part_t* part) {
  return sizeof(double);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free join part memory 
////////////////////////////////////////////////////////////////////////////////

static void FreeJoinPart (TRI_join_part_t* part) {
  if (part->_alias) {
    TRI_Free(part->_alias);
  }

  if (part->_ranges) {
    QLOptimizeFreeRangeVector(part->_ranges);
    TRI_Free(part->_ranges);
  }

  if (part->_collectionName) {
    TRI_Free(part->_collectionName);
  }
  
  if (part->_feeder) {
    part->_feeder->free(part->_feeder);
  }

  if (part->_context) {
    TRI_FreeExecutionContext(part->_context);
  }

  if (part->_listDocuments._buffer) {  
    TRI_DestroyVectorPointer(&part->_listDocuments);
  }

  TRI_Free(part);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a part to a select join
////////////////////////////////////////////////////////////////////////////////

static bool AddJoinPartQueryInstance (TRI_query_instance_t* instance, 
                                      const TRI_join_type_e type, 
                                      QL_ast_query_where_t* where,
                                      TRI_vector_pointer_t* ranges, 
                                      char* collectionName, 
                                      char* alias,
                                      QL_ast_query_geo_restriction_t* geoRestriction) {
  TRI_join_part_t* part;
  QL_ast_query_collection_t* collection;
  
  part = (TRI_join_part_t*) TRI_Allocate(sizeof(TRI_join_part_t));

  if (!part) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }
  
  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&instance->_query._from._collections, 
                                      alias);

  assert(collection);

  // initialize vector for list joins
  TRI_InitVectorPointer(&part->_listDocuments);
  part->_context = NULL;
  part->_singleDocument = NULL;
  part->_feeder = NULL; 
  part->_type = type;
  part->_where = where;
  part->_ranges = ranges;
  part->_collection = NULL;
  part->_collectionName = TRI_DuplicateString(collectionName);
  part->_alias = TRI_DuplicateString(alias);
  part->_geoRestriction = geoRestriction;
  part->_mustMaterialize._select = (collection->_refCount._select > 0);
  part->_mustMaterialize._where = (collection->_refCount._where > 0);
  part->_mustMaterialize._order = (collection->_refCount._order > 0);
  part->_mustMaterialize._join = (collection->_refCount._join > 0); 

  if (!part->_collectionName || !part->_alias) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    part->free(part);
    return false;
  }
  
  // determine size of extra data to store  
  if (part->_geoRestriction) {
    part->_extraData._size = GetJoinPartExtraDataSizeGeo(part);
    part->_extraData._alias = part->_geoRestriction->_alias;
  }
  else {
    part->_extraData._size = 0;
    part->_extraData._alias = NULL;
  }
  part->free = FreeJoinPart; 
  
  if (part->_where != NULL && 
      part->_where->_type == QLQueryWhereTypeMustEvaluate) {
    assert(part->_where->_functionCode);
    part->_context = TRI_CreateExecutionContext(part->_where->_functionCode);
    if (!part->_context) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
      part->free(part);
      return false;
    }
  }

  if (part->_where) {
    LOG_DEBUG("added join part to query instance. alias: %s, name: %s, "
              "type: %i, functionCode: %s",
              part->_alias,
              part->_collectionName,
              (int) part->_where->_type,
              (part->_where->_functionCode ? part->_where->_functionCode : ""));
  }
  else {
    LOG_DEBUG("added join part to query instance. alias: %s, name: %s",
              part->_alias,
              part->_collectionName);
  }

  TRI_PushBackVectorPointer(&instance->_join, part);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the select part of a query instance
////////////////////////////////////////////////////////////////////////////////

static bool InitSelectQueryInstance (TRI_query_instance_t* const instance) {
  QL_ast_query_select_t* queryTemplate = &instance->_template->_query->_select;
  QL_ast_query_select_t* queryInstance = &instance->_query._select;

  // clone original values
  queryInstance->_base               = queryTemplate->_base;
  queryInstance->_type               = queryTemplate->_type;
  queryInstance->_usesBindParameters = queryTemplate->_usesBindParameters;
  queryInstance->_functionCode       = queryTemplate->_functionCode;

  if (queryInstance->_usesBindParameters) {
    TRI_query_node_t* copy;
    char* functionCode;

    // if bind parameters are used, copy part and re-optimize
    assert(queryTemplate->_base);
    copy = TRI_CopyQueryPartQueryInstance(instance, queryTemplate->_base);
    if (!copy) {
      return false;
    }
    queryInstance->_base = copy;
    QLOptimizeExpression(queryInstance->_base);
    
    // re-create function code
    functionCode = TRI_GetFunctionCodeQueryJavascript(
      queryInstance->_base,
      &instance->_bindParameters
    );
    if (functionCode) {
      TRI_RegisterStringQuery(&instance->_memory._strings, functionCode);
      queryInstance->_functionCode = functionCode;
    }
    else {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }
  }
  
  LOG_DEBUG("added select part to query instance. type: %i, functionCode: %s",
            (int) queryInstance->_type,
            (queryInstance->_functionCode ? queryInstance->_functionCode : ""));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the where part of a query instance
////////////////////////////////////////////////////////////////////////////////

static bool InitWhereQueryInstance (TRI_query_instance_t* const instance) {
  QL_ast_query_where_t* queryTemplate = &instance->_template->_query->_where;
  QL_ast_query_where_t* queryInstance = &instance->_query._where;

  // clone original values
  queryInstance->_base               = queryTemplate->_base;
  queryInstance->_type               = queryTemplate->_type;
  queryInstance->_usesBindParameters = queryTemplate->_usesBindParameters;
  queryInstance->_functionCode       = queryTemplate->_functionCode;
  queryInstance->_context            = NULL;

  if (queryInstance->_usesBindParameters) {
    TRI_query_node_t* copy;

    // if bind parameters are used, copy part and re-optimize
    assert(queryTemplate->_base);
    copy = TRI_CopyQueryPartQueryInstance(instance, queryTemplate->_base);
    if (!copy) {
      return false;
    }
    queryInstance->_base = copy;
    QLOptimizeExpression(queryInstance->_base);

    queryInstance->_type = QLOptimizeGetWhereType(queryInstance->_base);

    if (queryInstance->_type == QLQueryWhereTypeMustEvaluate) {
      char* functionCode;

      // re-create function code
      functionCode = TRI_GetFunctionCodeQueryJavascript(
          queryInstance->_base,
          &instance->_bindParameters
          );
      if (functionCode) {
        TRI_RegisterStringQuery(&instance->_memory._strings, functionCode);
        queryInstance->_functionCode = functionCode;
      }
      else {
        TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return false;
      }
    }
  }

  if (queryInstance->_functionCode) {
    queryInstance->_context = TRI_CreateExecutionContext(queryInstance->_functionCode);
    if (!queryInstance->_context) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }
  }
  
  LOG_DEBUG("added where part to query instance. type: %i, functionCode: %s",
            (int) queryInstance->_type,
            (queryInstance->_functionCode ? queryInstance->_functionCode : ""));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the order part of a query instance
////////////////////////////////////////////////////////////////////////////////

static bool InitOrderQueryInstance (TRI_query_instance_t* const instance) {
  QL_ast_query_order_t* queryTemplate = &instance->_template->_query->_order;
  QL_ast_query_order_t* queryInstance = &instance->_query._order;

  // clone original values
  queryInstance->_base               = queryTemplate->_base;
  queryInstance->_type               = queryTemplate->_type;
  queryInstance->_usesBindParameters = queryTemplate->_usesBindParameters;
  queryInstance->_functionCode       = queryTemplate->_functionCode;

  if (queryTemplate->_usesBindParameters) {
    TRI_query_node_t* copy;

    // if bind parameters are used, copy part and re-optimize
    assert(queryTemplate->_base);
    copy = TRI_CopyQueryPartQueryInstance(instance, queryTemplate->_base);
    if (!copy) {
      return false;
    }
    queryInstance->_base = copy;
    QLOptimizeOrder(queryInstance->_base);
    queryInstance->_type = QLOptimizeGetOrderType(queryInstance->_base);

    if (queryInstance->_type == QLQueryOrderTypeMustEvaluate) {
      char* functionCode;

      // re-create function code
      functionCode = TRI_GetOrderFunctionCodeQueryJavascript(
          queryInstance->_base,
          &instance->_bindParameters
          );
      if (functionCode) {
        TRI_RegisterStringQuery(&instance->_memory._strings, functionCode);
        queryInstance->_functionCode = functionCode;
      }
      else {
        TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return false;
      }
    }
  }
  
  LOG_DEBUG("added order part to query instance. type: %i, functionCode: %s",
            (int) queryInstance->_type,
            (queryInstance->_functionCode ? queryInstance->_functionCode : ""));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the limit part of a query instance
////////////////////////////////////////////////////////////////////////////////

static bool InitLimitQueryInstance (TRI_query_instance_t* const instance) {
  QL_ast_query_limit_t* queryTemplate = &instance->_template->_query->_limit;
  QL_ast_query_limit_t* queryInstance = &instance->_query._limit;

  // clone original values
  queryInstance->_offset = queryTemplate->_offset;
  queryInstance->_count  = queryTemplate->_count;
  queryInstance->_isUsed = queryTemplate->_isUsed;

  if (!queryInstance->_isUsed) {
    queryInstance->_offset = 0;
    queryInstance->_count  = (TRI_voc_ssize_t) INT32_MAX;
  }
  
  LOG_DEBUG("added limit part to query instance. offset: %lu, count: %li",
            (unsigned long) queryInstance->_offset,
            (unsigned int) queryInstance->_count);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init the from part of a query instance
////////////////////////////////////////////////////////////////////////////////

static bool InitFromQueryInstance (TRI_query_instance_t* const instance) {
  QL_ast_query_from_t* queryTemplate = &instance->_template->_query->_from;
  size_t i;

  instance->_query._from._base = queryTemplate->_base;

  // clone original values
  for (i = 0; i < queryTemplate->_collections._nrAlloc; i++) {
    QL_ast_query_collection_t* source;
    QL_ast_query_collection_t* collection;

    source = (QL_ast_query_collection_t*) queryTemplate->_collections._table[i];
    if (!source) {
      continue;
    }

    collection = (QL_ast_query_collection_t*) 
      TRI_Allocate(sizeof(QL_ast_query_collection_t));
    if (!collection) { 
      return false;
    }

    collection->_name                       = source->_name;
    collection->_alias                      = source->_alias;
    collection->_isPrimary                  = source->_isPrimary;
    collection->_declarationOrder           = source->_declarationOrder;
    collection->_geoRestriction             = source->_geoRestriction;
    collection->_where._base                = source->_where._base;
    collection->_where._type                = source->_where._type;
    collection->_where._usesBindParameters  = source->_where._usesBindParameters;
    collection->_where._functionCode        = source->_where._functionCode;
    collection->_refCount                   = source->_refCount;

    if (source->_where._usesBindParameters) {
      TRI_query_node_t* copy;
      
      assert(source->_where._base);
      copy = TRI_CopyQueryPartQueryInstance(instance, source->_where._base);
      if (!copy) {
        TRI_Free(collection);
        TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return false;
      }

      collection->_where._base = copy;
      QLOptimizeExpression(collection->_where._base);
      collection->_where._type = QLOptimizeGetWhereType(collection->_where._base);

      if (collection->_where._type == QLQueryWhereTypeMustEvaluate) {
        char* functionCode;

        // re-create function code
        functionCode = TRI_GetFunctionCodeQueryJavascript(
            collection->_where._base,
            &instance->_bindParameters
            );
        if (functionCode) {
          TRI_RegisterStringQuery(&instance->_memory._strings, functionCode);
          collection->_where._functionCode = functionCode;
        }
        else {
          TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return false;
        }
      }
    }

    TRI_InsertKeyAssociativePointer(&instance->_query._from._collections,
                                    collection->_alias,
                                    (void*) collection, 
                                    true);
  
    LOG_DEBUG("added join part to query instance. alias: %s, name: %s, "
              "type: %i, usesBindParameters: %i, functionCode: %s",
              collection->_alias,
              collection->_name,
              (int) collection->_where._type,
              (int) collection->_where._usesBindParameters,
              (collection->_where._functionCode ? collection->_where._functionCode : ""));
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Init query parts of the instance
///
/// If the query uses bind parameters, the query parts with bind parameters are
/// copied, re-optimized, and the Javascript functions for these parts are
/// re-created.
////////////////////////////////////////////////////////////////////////////////
  
static bool InitPartsQueryInstance (TRI_query_instance_t* const instance) {
  QL_ast_query_t* queryTemplate;
  QL_ast_query_t* queryInstance;

  assert(instance);
  assert(instance->_template);
  assert(instance->_template->_query);

  queryTemplate = instance->_template->_query;
  queryInstance = &instance->_query;

  queryInstance->_isEmpty = queryTemplate->_isEmpty;

  if (!InitSelectQueryInstance(instance)) {
    return false;
  }

  if (!InitWhereQueryInstance(instance)) {
    return false;
  }
  
  if (!InitOrderQueryInstance(instance)) {
    return false;
  }
  
  if (!InitLimitQueryInstance(instance)) {
    return false;
  }
  
  if (!InitFromQueryInstance(instance)) {
    return false;
  }
 
  if (queryInstance->_where._type == QLQueryWhereTypeAlwaysFalse) {
    // query will never have any results due to where clause
    queryInstance->_isEmpty = true;
  }

  if (queryInstance->_limit._count == 0) {
    // query will never have any results due to limit clause
    queryInstance->_isEmpty = true;
  }
    
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get join type from a join node
////////////////////////////////////////////////////////////////////////////////

static TRI_join_type_e GetJoinTypeFromNode (const TRI_query_node_t* const node) {
  switch (node->_type) {
    case TRI_QueryNodeJoinList:
      return JOIN_TYPE_LIST;
    case TRI_QueryNodeJoinInner:
      return JOIN_TYPE_INNER;
    case TRI_QueryNodeJoinLeft:
      return JOIN_TYPE_OUTER;
    default:
      assert(false);
  }

  assert(false);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add primary collection to join 
////////////////////////////////////////////////////////////////////////////////

static bool AddJoinPrimaryCollection (TRI_query_instance_t* const instance,  
                                      const TRI_query_node_t* const node) {
  TRI_vector_pointer_t* ranges;
  QL_ast_query_collection_t* collection;
  char* collectionName;
  char* collectionAlias;

  assert(instance);
  assert(node);
  assert(node->_lhs);
  assert(node->_rhs);
  
  collectionName = node->_lhs->_value._stringValue;
  collectionAlias = node->_rhs->_value._stringValue;

  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&instance->_query._from._collections, 
                                      collectionAlias);

  assert(collection);

  ranges = NULL;
  if (!collection->_geoRestriction) {
    ranges = QLOptimizeRanges(instance->_query._where._base, 
                              &instance->_bindParameters);
  }

  return AddJoinPartQueryInstance(instance, 
                                 JOIN_TYPE_PRIMARY, 
                                 NULL, 
                                 ranges, 
                                 collectionName, 
                                 collectionAlias,
                                 QLAstQueryCloneRestriction(collection->_geoRestriction));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add another (non-primary) collection to join 
////////////////////////////////////////////////////////////////////////////////

static bool AddJoinSecondaryCollection (TRI_query_instance_t* const instance,  
                                        const TRI_query_node_t* const node) {
  TRI_vector_pointer_t* ranges;
  QL_ast_query_collection_t* collection;
  QL_ast_query_where_type_e conditionType;
  char* collectionName;
  char* collectionAlias;

  assert(instance);
  assert(node);
    
  collectionName = node->_lhs->_lhs->_value._stringValue;
  collectionAlias = node->_lhs->_rhs->_value._stringValue;

  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&instance->_query._from._collections, 
                                      collectionAlias);

  assert(collection);
    
  ranges = NULL;

  conditionType = collection->_where._type;
  if (conditionType == QLQueryWhereTypeMustEvaluate) {
    // join condition must be evaluated for each result
    if (!collection->_geoRestriction) {
      ranges = QLOptimizeRanges(node->_rhs, &instance->_bindParameters);
    }
  }

  return AddJoinPartQueryInstance(instance, 
                                  GetJoinTypeFromNode(node), 
                                  &collection->_where,
                                  ranges, 
                                  collectionName, 
                                  collectionAlias,
                                  QLAstQueryCloneRestriction(collection->_geoRestriction));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Set up the join part of the query
////////////////////////////////////////////////////////////////////////////////

static bool InitJoinsQueryInstance (TRI_query_instance_t* const instance) {
  TRI_query_node_t* node;

  assert(instance);
  assert(instance->_query._from._base);

  node = instance->_query._from._base->_next;
  assert(node);

  AddJoinPrimaryCollection(instance, node);

  node = node->_next;

  while (node) {
    if (!AddJoinSecondaryCollection(instance, node)) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }

    node = node->_next;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free join parts used in a query
////////////////////////////////////////////////////////////////////////////////

static void FreeJoinsQueryInstance (TRI_query_instance_t* const instance) {
  size_t i;

  for (i = 0; i < instance->_join._length; i++) {
    TRI_join_part_t* part;

    part = (TRI_join_part_t*) instance->_join._buffer[i];
    assert(part);
    part->free(part);
  }

  TRI_DestroyVectorPointer(&instance->_join);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Set up the join part of the query
////////////////////////////////////////////////////////////////////////////////

static bool InitCollectionsQueryInstance (TRI_query_instance_t* const instance) {
  TRI_vocbase_t* vocbase;
  size_t i;

  assert(instance);
  
  vocbase = instance->_template->_vocbase;
  assert(vocbase);

  for (i = 0 ; i < instance->_join._length; i++) {
    TRI_join_part_t* part;
    TRI_vocbase_col_t const* collection;

    part = (TRI_join_part_t*) instance->_join._buffer[i];
    assert(!part->_collection);
    assert(part->_collectionName);
    collection = TRI_LoadCollectionVocBase(vocbase, part->_collectionName);
    if (!collection) {
      TRI_RegisterErrorQueryInstance(instance, 
      TRI_ERROR_QUERY_COLLECTION_NOT_FOUND, 
      part->_collectionName);
      return false;
    }
    part->_collection = (TRI_doc_collection_t*) collection->_collection;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Set up the locks for the query
////////////////////////////////////////////////////////////////////////////////

static bool InitLocksQueryInstance (TRI_query_instance_t* const instance) {
  size_t i;

  assert(instance);

  for (i = 0; i < instance->_join._length; i++) {
    TRI_join_part_t* part = (TRI_join_part_t*) instance->_join._buffer[i];
    TRI_query_instance_lock_t* lock;
    bool insert;
    size_t j;

    assert(part);

    insert = true;
    for (j = 0; j < instance->_locks._length; j++) {
      int compareResult;

      lock = (TRI_query_instance_lock_t*) instance->_locks._buffer[j];
      assert(lock);
      assert(lock->_collection);
      assert(lock->_collectionName);

      compareResult = strcmp(part->_collectionName, lock->_collectionName);

      if (compareResult == 0) {
        insert = (compareResult > 0);
        break;
      }
    }

    if (insert) {
      lock = (TRI_query_instance_lock_t*) TRI_Allocate(sizeof(TRI_query_instance_lock_t));
      if (!lock) {
        TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return false;
      }

      lock->_collection = part->_collection;
      lock->_collectionName = TRI_DuplicateString(part->_collectionName);
      if (!lock->_collectionName) {
        TRI_Free(lock);
        TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return false;
      }
      TRI_InsertVectorPointer(&instance->_locks, lock, j);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the locks for the query
////////////////////////////////////////////////////////////////////////////////

static void FreeLocksQueryInstance (TRI_query_instance_t* const instance) {
  size_t i;
  
  for (i = 0; i < instance->_locks._length; i++) {
    TRI_query_instance_lock_t* lock = 
      (TRI_query_instance_lock_t*) instance->_locks._buffer[i];

    assert(lock);
    assert(lock->_collectionName);

    TRI_Free(lock->_collectionName);
    TRI_Free(lock);
  }

  TRI_DestroyVectorPointer(&instance->_locks);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add bind parameter values
////////////////////////////////////////////////////////////////////////////////

static bool AddBindParameterValues (TRI_query_instance_t* const instance, 
                                    const TRI_json_t* parameters) {
  TRI_bind_parameter_t* parameter;
  void* nameParameter;
  void* valueParameter;
  size_t i;

  assert(parameters);

  if (parameters->_type != TRI_JSON_ARRAY) {
    TRI_RegisterErrorQueryInstance(instance, 
                                   TRI_ERROR_QUERY_BIND_PARAMETER_MISSING, 
                                   "global");
    return false;
  }
    
  for (i = 0; i < parameters->_value._objects._length; i += 2) {
    nameParameter = TRI_AtVector(&parameters->_value._objects, i);
    valueParameter = TRI_AtVector(&parameters->_value._objects, i + 1);

    parameter = 
      TRI_CreateBindParameter(((TRI_json_t*) nameParameter)->_value._string.data, 
      (TRI_json_t*) valueParameter);

    if (!parameter) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return false;
    }
    
    if (TRI_LookupByKeyAssociativePointer(&instance->_bindParameters, 
                                          parameter->_name)) {
      TRI_FreeBindParameter(parameter);
      // redeclaration of bind parameter
      TRI_RegisterErrorQueryInstance(instance, 
                                     TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED, 
                                     parameter->_name);
      return false;
    }

    TRI_AddBindParameterQueryInstance(instance, parameter);
  
    // check if template has such bind parameter
    if (!TRI_LookupByKeyAssociativePointer(&instance->_template->_bindParameters, 
                                           parameter->_name)) {
      // invalid bind parameter
      TRI_RegisterErrorQueryInstance(instance, 
                                     TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED, 
                                     parameter->_name);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate bind parameter values
////////////////////////////////////////////////////////////////////////////////

static bool ValidateBindParameters (TRI_query_instance_t* const instance) {
  TRI_associative_pointer_t* templateParameters;
  TRI_associative_pointer_t* instanceParameters;
  TRI_bind_parameter_t* parameter;
  size_t i;

  templateParameters = &instance->_template->_bindParameters;
  instanceParameters = &instance->_bindParameters;

  // enumerate all template bind parameters....
  for (i = 0; i < templateParameters->_nrAlloc; i++) {
    parameter = (TRI_bind_parameter_t*) templateParameters->_table[i];
    if (!parameter) {
      continue;
    }

    // ... and check if the parameter is also defined for the instance
    if (!TRI_LookupByKeyAssociativePointer(instanceParameters,
                                           parameter->_name)) {
      // invalid bind parameter
      TRI_RegisterErrorQueryInstance(instance, 
                                     TRI_ERROR_QUERY_BIND_PARAMETER_MISSING,
                                     parameter->_name);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Set the value of a bind parameter
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBindParameterQueryInstance (TRI_query_instance_t* const instance,
                                        const TRI_bind_parameter_t* const parameter) {
  assert(instance);
  assert(parameter);
  assert(parameter->_name);
  assert(parameter->_data);

  // check if template has bind parameter defined
  if (!TRI_LookupByKeyAssociativePointer(&instance->_template->_bindParameters, 
                                         parameter->_name)) {
    // invalid bind parameter
    TRI_RegisterErrorQueryInstance(instance, 
                                   TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED, 
                                   parameter->_name);
    return false;
  }
  
  // check if bind parameter is already defined for instance
  if (TRI_LookupByKeyAssociativePointer(&instance->_bindParameters, 
                                        parameter->_name)) {
    // bind parameter defined => duplicate definition
    TRI_RegisterErrorQueryInstance(instance, 
                                   TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED, 
                                   parameter->_name);
    return false;
  }

  TRI_InsertKeyAssociativePointer(&instance->_bindParameters, 
                                  parameter->_name,
                                  (void*) parameter, 
                                  true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Free a query instance
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryInstance (TRI_query_instance_t* const instance) {
  assert(instance);
  assert(instance->_template);
  
  TRI_FreeQueryError(&instance->_error); 

  FreeBindParameters(&instance->_bindParameters);
  TRI_DestroyAssociativePointer(&instance->_bindParameters);
  TRI_FreeNodeVectorQuery(&instance->_memory._nodes);
  TRI_FreeStringVectorQuery(&instance->_memory._strings);
  QLAstQueryFreeCollections(&instance->_query._from._collections);

  if (instance->_query._where._context) {
    TRI_FreeExecutionContext(instance->_query._where._context);
  }

  FreeJoinsQueryInstance(instance);
  FreeLocksQueryInstance(instance);

  TRI_Free(instance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a query instance with bind parameters (may be empty)
////////////////////////////////////////////////////////////////////////////////

TRI_query_instance_t* TRI_CreateQueryInstance (const TRI_query_template_t* const template_,
                                               const TRI_json_t* parameters) {
  TRI_query_instance_t* instance;

  assert(template_);

  instance = (TRI_query_instance_t*) TRI_Allocate(sizeof(TRI_query_instance_t));
  if (!instance) {
    return NULL;
  }

  // init values
  instance->_template  = (TRI_query_template_t*) template_;
  instance->_wasKilled = false;
  instance->_doAbort   = false;
  
  TRI_InitQueryError(&instance->_error);
  
  // init vectors needed for book-keeping memory
  TRI_InitVectorPointer(&instance->_memory._nodes);
  TRI_InitVectorPointer(&instance->_memory._strings);
  
  TRI_InitVectorPointer(&instance->_join);
  TRI_InitVectorPointer(&instance->_locks);

  TRI_InitAssociativePointer(&instance->_bindParameters,
                             HashBindParameterName,
                             HashBindParameter,
                             EqualBindParameter,
                             0);
  
  TRI_InitAssociativePointer(&instance->_query._from._collections,
                             QLHashKey,
                             QLHashCollectionElement,
                             QLEqualCollectionKeyElement,
                             0);
 
  // set up bind parameters
  if (parameters) {
    if (!AddBindParameterValues(instance, parameters)) {
      return instance;
    }
  }

  // validate bind parameters
  if (!ValidateBindParameters(instance)) {
    return instance;
  }

  if (!InitPartsQueryInstance(instance)) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return instance;
  }

  if (!InitJoinsQueryInstance(instance)) {
    return instance;
  }

  if (!InitCollectionsQueryInstance(instance)) {
    return instance;
  }

  if (!InitLocksQueryInstance(instance)) {
    return instance;
  }

  return instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Kill a query instance
////////////////////////////////////////////////////////////////////////////////

void TRI_KillQueryInstance (TRI_query_instance_t* const instance) {
  assert(instance);

  instance->_wasKilled = true;
  TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_QUERY_KILLED, NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Register an error during query execution
////////////////////////////////////////////////////////////////////////////////

bool TRI_RegisterErrorQueryInstance (TRI_query_instance_t* const instance, 
                                     const int code,
                                     const char* data) {

  assert(instance);
  assert(code > 0);

  TRI_SetQueryError(&instance->_error, code, data);

  // set abort flag
  instance->_doAbort = true;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a node from a json data part
////////////////////////////////////////////////////////////////////////////////

static TRI_query_node_t* CreateNodeFromJson (TRI_query_instance_t* const instance,
                                             const TRI_json_t* json) {
  TRI_query_node_t* node;
  TRI_query_node_t* nodePtr;
  char* stringValue;
  size_t i, n;

  assert(instance);

  node = TRI_CreateNodeQuery(&instance->_memory._nodes, TRI_QueryNodeValueUndefined);
  if (!node) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  switch (json->_type) {
    case TRI_JSON_UNUSED:
      break;

    case TRI_JSON_NULL:
      node->_type = TRI_QueryNodeValueNull;
      break;

    case TRI_JSON_BOOLEAN:
      node->_type = TRI_QueryNodeValueBool;
      node->_value._boolValue = json->_value._boolean;
      break;

    case TRI_JSON_NUMBER:
      node->_type = TRI_QueryNodeValueNumberDouble;
      node->_value._doubleValue = json->_value._number;
      break;

    case TRI_JSON_STRING:
      stringValue = TRI_DuplicateString(json->_value._string.data);
      if (stringValue) {
        node->_type = TRI_QueryNodeValueString;
        node->_value._stringValue = stringValue;
        TRI_RegisterStringQuery(&instance->_memory._strings, stringValue);
      }
      else {
        TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return NULL;
      }
      break;

    case TRI_JSON_ARRAY:
      node->_type = TRI_QueryNodeValueDocument;
      // create a linked list of sub nodes
      n = json->_value._objects._length;
      if (n > 0) {
        TRI_query_node_t* container;
        
        container = TRI_CreateNodeQuery(&instance->_memory._nodes, 
                                        TRI_QueryNodeContainerList);
        if (!container) {
          TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }
        node->_rhs = container;
        nodePtr = container;

        for (i = 0; i < n; i += 2) {
          TRI_query_node_t* namedValueNode;

          namedValueNode = TRI_CreateNodeQuery(&instance->_memory._nodes, 
                                               TRI_QueryNodeValueNamedValue);
          if (!namedValueNode) {
            TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
            return NULL;
          }

          namedValueNode->_lhs = CreateNodeFromJson(
            instance, 
            (TRI_json_t*) TRI_AtVector(&json->_value._objects, i)
          );

          namedValueNode->_rhs = CreateNodeFromJson(
            instance, 
            (TRI_json_t*) TRI_AtVector(&json->_value._objects, i + 1)
          );

          if (!namedValueNode->_lhs || !namedValueNode->_rhs) {
            TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
            return NULL;
          }

          nodePtr->_next = namedValueNode;
          nodePtr = namedValueNode;
        }
      }
      break;

    case TRI_JSON_LIST:
      node->_type = TRI_QueryNodeValueArray;
      // create a linked list of sub nodes
      n = json->_value._objects._length;
      if (n > 0) {
        TRI_query_node_t* container;

        container = TRI_CreateNodeQuery(&instance->_memory._nodes, 
                                        TRI_QueryNodeContainerList);
        if (!container) {
          TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
          return NULL;
        }
        node->_rhs = container;
        nodePtr = container;
        for (i = 0; i < n; i++) {
          TRI_query_node_t* subNode;
          subNode = CreateNodeFromJson(
            instance, 
            (TRI_json_t*) TRI_AtVector(&json->_value._objects, i)
          );

          if (!subNode) {
            TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
            return NULL;
          }

          nodePtr->_next = subNode;
          nodePtr = subNode;
        }
      }
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a node from a bind parameter
////////////////////////////////////////////////////////////////////////////////

static TRI_query_node_t* CreateNodeFromBindParameter (TRI_query_instance_t* const instance,
                                                      const TRI_query_node_t* const node) {
  TRI_bind_parameter_t* parameter;
                        
  // node is a bind parameter, now insert bind parameter values
  assert(node->_value._stringValue);
  parameter = (TRI_bind_parameter_t*) TRI_LookupByKeyAssociativePointer(
    &instance->_bindParameters, 
    node->_value._stringValue
  );

  assert(parameter);
  assert(parameter->_data);
  
  return CreateNodeFromJson(instance, parameter->_data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Copy a part of the query's AST and insert bind parameter values on
/// the fly
////////////////////////////////////////////////////////////////////////////////

TRI_query_node_t* TRI_CopyQueryPartQueryInstance (TRI_query_instance_t* const instance,
                                                  const TRI_query_node_t* const node) {
  TRI_query_node_t* copy;
  TRI_query_node_t* next;
  
  if (node->_type == TRI_QueryNodeValueParameterNamed) {
    return CreateNodeFromBindParameter(instance, node);
  }
  
  copy = TRI_CreateNodeQuery(&instance->_memory._nodes, node->_type);
  if (!copy) {
    TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }
  copy->_value = node->_value;

  next = node->_next;
  while (next) {
    copy->_next = TRI_CopyQueryPartQueryInstance(instance, next);
    if (!copy->_next) {
      TRI_RegisterErrorQueryInstance(instance, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return NULL;
    }
    next = next->_next;
  }

  if (node->_lhs) {
    copy->_lhs = TRI_CopyQueryPartQueryInstance(instance, node->_lhs);
  }
  if (node->_rhs) {
    copy->_rhs = TRI_CopyQueryPartQueryInstance(instance, node->_rhs);
  }

  return copy;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            errors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Register an error
////////////////////////////////////////////////////////////////////////////////

void TRI_SetQueryError (TRI_query_error_t* const error, 
                        const int code, 
                        const char* data) {
  assert(code > 0);

  if (TRI_GetCodeQueryError(error) == 0) {
    // do not overwrite previous error
    TRI_set_errno(code);
    error->_code = code;
    error->_message = (char*) TRI_last_error();
    if (data) {
      error->_data = TRI_DuplicateString(data);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the error code registered last
////////////////////////////////////////////////////////////////////////////////

int TRI_GetCodeQueryError (const TRI_query_error_t* const error) {
  assert(error);
  
  return error->_code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the error string registered last
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetStringQueryError (const TRI_query_error_t* const error) {
  char* message = NULL;
  char buffer[1024];
  int code;

  assert(error);
  code = TRI_GetCodeQueryError(error);
  if (!code) {
    return NULL;
  }

  message = error->_message;
  if (!message) {
    return NULL;
  }

  if (error->_data && (NULL != strstr(message, "%s"))) {
    snprintf(buffer, sizeof(buffer), message, error->_data);
    return TRI_DuplicateString((const char*) &buffer);
  }

  return TRI_DuplicateString(message);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize an error structure
////////////////////////////////////////////////////////////////////////////////
  
void TRI_InitQueryError (TRI_query_error_t* const error) {
  assert(error);

  error->_code = 0;
  error->_data = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an error structure
////////////////////////////////////////////////////////////////////////////////
  
void TRI_FreeQueryError (TRI_query_error_t* const error) {
  assert(error);
  
  if (error->_data) {
    TRI_Free(error->_data);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
