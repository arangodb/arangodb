////////////////////////////////////////////////////////////////////////////////
/// @brief AST query declarations
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

#include "QL/ast-query.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash collection aliases
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey (TRI_associative_pointer_t* array, void const* key) {
  char const* k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash elements in the collection
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashCollectionElement (TRI_associative_pointer_t* array, 
                                       void const* element) {
  QL_ast_query_collection_t* collection = (QL_ast_query_collection_t*) element;

  return TRI_FnvHashString(collection->_alias);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Comparison function used to determine hash key equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualCollectionKeyElement (TRI_associative_pointer_t* array, 
                                       void const* key, 
                                       void const* element) {
  char const* k = (char const*) key;
  QL_ast_query_collection_t* collection = (QL_ast_query_collection_t*) element;

  return TRI_EqualString(k, collection->_alias);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash function used to hash geo restrictions
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashRestrictionElement (TRI_associative_pointer_t* array, 
                                        void const* element) {
  QL_ast_query_geo_restriction_t* restriction = 
    (QL_ast_query_geo_restriction_t*) element;

  return TRI_FnvHashString(restriction->_alias);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Comparison function used to determine hash key equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualRestrictionKeyElement (TRI_associative_pointer_t* array, 
                                        void const* key, 
                                        void const* element) {
  char const* k = (char const*) key;
  QL_ast_query_geo_restriction_t* restriction = 
    (QL_ast_query_geo_restriction_t*) element;

  return TRI_EqualString(k, restriction->_alias);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free geo restrictions previously registered
////////////////////////////////////////////////////////////////////////////////

static void QLAstQueryFreeGeoRestrictions (TRI_associative_pointer_t* const array) {
  QL_ast_query_geo_restriction_t* restriction;
  size_t i; 
  
  // destroy all elements in restrictions array
  for (i = 0; i < array->_nrAlloc; i++) {
    restriction = (QL_ast_query_geo_restriction_t*) array->_table[i];
    if (restriction) {
      QLAstQueryFreeRestriction(restriction);
    }
  }
  // destroy restrictions array
  TRI_DestroyAssociativePointer(array);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief free collections previously registered
////////////////////////////////////////////////////////////////////////////////

void QLAstQueryFreeCollections (TRI_associative_pointer_t* const array) {
  QL_ast_query_collection_t* collection;
  size_t i; 

  // destroy all elements in collection array
  for (i = 0; i < array->_nrAlloc; i++) {
    collection = (QL_ast_query_collection_t*) array->_table[i];
    if (collection) {
      TRI_Free(collection);
    }
  }
  // destroy collection array itself
  TRI_DestroyAssociativePointer(array);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Initialize data structures for a query
////////////////////////////////////////////////////////////////////////////////

void QLAstQueryInit (QL_ast_query_t* const query) {
  query->_type                       = QLQueryTypeUndefined;

  query->_select._base               = NULL;
  query->_select._functionCode       = NULL;
  query->_select._usesBindParameters = false;

  query->_from._base                 = NULL;

  query->_where._base                = NULL;
  query->_where._functionCode        = NULL;
  query->_where._usesBindParameters  = false;

  query->_order._base                = NULL;
  query->_order._functionCode        = NULL;
  query->_order._usesBindParameters  = false;

  query->_limit._isUsed              = false;

  query->_isEmpty                    = false;

  TRI_InitAssociativePointer(&query->_from._collections,
                             HashKey,
                             HashCollectionElement,
                             EqualCollectionKeyElement,
                             0);
  
  TRI_InitAssociativePointer(&query->_geo._restrictions,
                             HashKey,
                             HashRestrictionElement,
                             EqualRestrictionKeyElement,
                             0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief De-allocate data structures for a query
////////////////////////////////////////////////////////////////////////////////

void QLAstQueryFree (QL_ast_query_t* const query) {
  // free collections data
  QLAstQueryFreeCollections(&query->_from._collections);

  // free geo restrictions data
  QLAstQueryFreeGeoRestrictions(&query->_geo._restrictions);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the ref count for a collection
////////////////////////////////////////////////////////////////////////////////

size_t QLAstQueryGetRefCount (QL_ast_query_t* query, const char* alias) {
  QL_ast_query_collection_t* collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&query->_from._collections, alias);

  if (collection != 0) {
    return collection->_refCount;
  }

  // return a number > 0 if collection not found so nothing happens with it
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Increment ref count for a collection
////////////////////////////////////////////////////////////////////////////////

void QLAstQueryAddRefCount (QL_ast_query_t* query, const char* alias) {
  QL_ast_query_collection_t* collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&query->_from._collections, alias);

  if (collection != 0) {
    ++collection->_refCount;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Check if a collection was defined in a query
////////////////////////////////////////////////////////////////////////////////

bool QLAstQueryIsValidAlias (QL_ast_query_t* query, 
                             const char* alias, 
                             const size_t order) {
  if (0 != TRI_LookupByKeyAssociativePointer(&query->_from._collections, alias)) {
    return true;
  }

  return (0 != TRI_LookupByKeyAssociativePointer(&query->_geo._restrictions, alias));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Check if a collection was defined in a query, taking order of 
/// declaration into account
////////////////////////////////////////////////////////////////////////////////

bool QLAstQueryIsValidAliasOrdered (QL_ast_query_t* query, 
                                    const char* alias, 
                                    const size_t order) {
  QL_ast_query_collection_t* collection;

  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&query->_from._collections, alias);

  if (!collection) {
    return false;
  }

  return (collection->_declarationOrder <= order);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return the collection name for its alias
////////////////////////////////////////////////////////////////////////////////

char* QLAstQueryGetCollectionNameForAlias (QL_ast_query_t* query, 
                                           const char* alias) {
  QL_ast_query_collection_t* collection;

  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&query->_from._collections, alias);
  if (!collection) {
    return NULL;
  }
  return collection->_name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Add a collection to the query
////////////////////////////////////////////////////////////////////////////////

bool QLAstQueryAddCollection (QL_ast_query_t* query, 
                              const char* name, 
                              const char* alias) { 
  QL_ast_query_collection_t* collection;
  size_t num;

  if (TRI_LookupByKeyAssociativePointer(&query->_from._collections, alias)) {
    // alias has already been declared for another collection - return an error
    return false;
  }

  if (TRI_LookupByKeyAssociativePointer(&query->_geo._restrictions, alias)) {
    // alias has already been declared for a geo restriction - return an error
    return false;
  }

  collection = (QL_ast_query_collection_t*) 
    TRI_Allocate(sizeof(QL_ast_query_collection_t));
  if (!collection) { 
    return false;
  }

  num = query->_from._collections._nrUsed;

  collection->_name                      = (char*) name;
  collection->_alias                     = (char*) alias;
  // first collection added is always the primary collection
  collection->_isPrimary                 = (num == 0);
  collection->_refCount                  = 0; // will be used later when optimizing joins
  collection->_declarationOrder          = num + 1;
  collection->_geoRestriction            = NULL;

  collection->_where._base               = NULL;
  collection->_where._type               = QLQueryWhereTypeUndefined;
  collection->_where._usesBindParameters = false;
  collection->_where._functionCode       = NULL;

  TRI_InsertKeyAssociativePointer(&query->_from._collections, 
                                  collection->_alias, 
                                  collection, 
                                  true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the alias of the primary collection used in the query
////////////////////////////////////////////////////////////////////////////////

char* QLAstQueryGetPrimaryAlias (const QL_ast_query_t* query) {
  size_t i;
  QL_ast_query_collection_t *collection;

  for (i = 0; i < query->_from._collections._nrAlloc; i++) {
    collection = query->_from._collections._table[i];
    if (collection != 0 && collection->_isPrimary) {
      return collection->_alias;
    }
  }
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a geo restriction prototype
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_geo_restriction_t* QLAstQueryCreateRestriction (void) {
  QL_ast_query_geo_restriction_t* restriction;
  
  restriction = (QL_ast_query_geo_restriction_t*) 
    TRI_Allocate(sizeof(QL_ast_query_geo_restriction_t));
  if (!restriction) {
    return NULL;
  }
  restriction->_alias                  = NULL;
  restriction->_compareLat._collection = NULL;
  restriction->_compareLat._field      = NULL;
  restriction->_compareLon._collection = NULL;
  restriction->_compareLon._field      = NULL;

  return restriction;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a geo restriction 
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_geo_restriction_t* QLAstQueryCloneRestriction 
  (const QL_ast_query_geo_restriction_t* source) {
  QL_ast_query_geo_restriction_t* dest;
 
  if (!source) {
    return NULL;
  }
   
  dest = QLAstQueryCreateRestriction();
  if (!dest) {
    return NULL;
  }
  
  dest->_type = source->_type;
  dest->_alias = TRI_DuplicateString(source->_alias);
  dest->_compareLat._collection = TRI_DuplicateString(source->_compareLat._collection);
  dest->_compareLat._field = TRI_DuplicateString(source->_compareLat._field);
  dest->_compareLon._collection = TRI_DuplicateString(source->_compareLon._collection);
  dest->_compareLon._field = TRI_DuplicateString(source->_compareLon._field);
  dest->_lat = source->_lat;
  dest->_lon = source->_lon;
  dest->_arg = source->_arg;

  return dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a geo restriction 
////////////////////////////////////////////////////////////////////////////////
      
void QLAstQueryFreeRestriction (QL_ast_query_geo_restriction_t* restriction) {
  if (restriction->_compareLat._collection) {
    TRI_Free(restriction->_compareLat._collection);
  }
  if (restriction->_compareLat._field) {
    TRI_Free(restriction->_compareLat._field);
  }
  if (restriction->_compareLon._collection) {
    TRI_Free(restriction->_compareLon._collection);
  }
  if (restriction->_compareLon._field) {
    TRI_Free(restriction->_compareLon._field);
  }
  TRI_Free(restriction);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a geo restriction 
////////////////////////////////////////////////////////////////////////////////

bool QLAstQueryAddGeoRestriction (QL_ast_query_t* query,
                                  const TRI_query_node_t* collectionNode,
                                  const TRI_query_node_t* restrictionNode) {
  TRI_query_node_t* valueNode;
  QL_ast_query_collection_t* collection;
  QL_ast_query_geo_restriction_t* restriction;
  TRI_string_buffer_t* fieldName;
  char* alias;
  char* collectionAlias;
  void* previous;

  if (!collectionNode || !restrictionNode) {
    return false;
  }

  alias = restrictionNode->_lhs->_value._stringValue;
  assert(alias);

  previous = (void*) TRI_LookupByKeyAssociativePointer(&query->_from._collections, 
                                                       alias);
  if (previous) {
    // alias is already used for another collection
    return false;
  }

  previous = (void*) TRI_LookupByKeyAssociativePointer(&query->_geo._restrictions, 
                                                       alias);
  if (previous) {
    // alias is already used for another geo restriction 
    return false;
  }

  collectionAlias = ((TRI_query_node_t*) collectionNode->_rhs)->_value._stringValue;
  assert(collectionAlias);

  collection = (QL_ast_query_collection_t*) 
    TRI_LookupByKeyAssociativePointer(&query->_from._collections, collectionAlias);

  if (!collection) {
    // collection is not present - should not happen
    return false;
  }

  assert(restrictionNode->_lhs);
  assert(restrictionNode->_rhs);

  restriction = QLAstQueryCreateRestriction();
  if (!restriction) {
    return false;
  }
  restriction->_alias = alias;

  if (restrictionNode->_type == TRI_QueryNodeRestrictWithin) {
    restriction->_type = RESTRICT_WITHIN;
  }
  else {
    restriction->_type = RESTRICT_NEAR;
  }

  // compare field 1
  valueNode = restrictionNode->_rhs->_lhs->_lhs;
  if (strcmp(valueNode->_lhs->_value._stringValue,
             collectionAlias) != 0) {
    // field is from other collection, not valid!
    QLAstQueryFreeRestriction(restriction);
    return false;
  }

  fieldName = QLAstQueryGetMemberNameString(valueNode, false);
  if (fieldName) {
    restriction->_compareLat._collection = 
      TRI_DuplicateString(valueNode->_lhs->_value._stringValue);
    restriction->_compareLat._field = TRI_DuplicateString(fieldName->_buffer);
    TRI_FreeStringBuffer(fieldName);
    TRI_Free(fieldName);
  }
  else {
    QLAstQueryFreeRestriction(restriction);
    return false;
  }

  // compare field 2
  valueNode = restrictionNode->_rhs->_lhs->_rhs;
  if (strcmp(valueNode->_lhs->_value._stringValue,
             collectionAlias) != 0) {
    // field is from other collection, not valid!
    QLAstQueryFreeRestriction(restriction);
    return false;
  }

  fieldName = QLAstQueryGetMemberNameString(valueNode, false);
  if (fieldName) {
    restriction->_compareLon._collection = 
      TRI_DuplicateString(valueNode->_lhs->_value._stringValue);
    restriction->_compareLon._field = TRI_DuplicateString(fieldName->_buffer);
    TRI_FreeStringBuffer(fieldName);
    TRI_Free(fieldName);
  } 
  else {
    QLAstQueryFreeRestriction(restriction);
    return false;
  }

  // lat value
  valueNode = restrictionNode->_rhs->_rhs->_lhs;
  restriction->_lat = valueNode->_value._doubleValue;  

  // lon value
  valueNode = restrictionNode->_rhs->_rhs->_rhs;
  restriction->_lon = valueNode->_value._doubleValue;  

  if (restrictionNode->_type == TRI_QueryNodeRestrictWithin) {
    restriction->_arg._radius = restrictionNode->_value._doubleValue;
  }
  else {
    restriction->_arg._numDocuments = restrictionNode->_value._intValue;
  }

  TRI_InsertKeyAssociativePointer(&query->_geo._restrictions, 
      alias, 
      restriction, 
      true);

  collection->_geoRestriction = restriction;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a string from a member name
////////////////////////////////////////////////////////////////////////////////

TRI_string_buffer_t* QLAstQueryGetMemberNameString (TRI_query_node_t* node, 
                                                    bool includeCollection) {
  TRI_query_node_t *lhs, *rhs;
  TRI_string_buffer_t* buffer;

  buffer = (TRI_string_buffer_t*) TRI_Allocate(sizeof(TRI_string_buffer_t));
  if (!buffer) {
    return NULL;
  }

  TRI_InitStringBuffer(buffer);

  if (includeCollection) {
    // add collection part
    lhs = node->_lhs;
    TRI_AppendStringStringBuffer(buffer, lhs->_value._stringValue);
    TRI_AppendCharStringBuffer(buffer, '.');
  }
  
  rhs = node->_rhs;
  node = rhs->_next;

  while (node) {
    // add individual name parts
    TRI_AppendStringStringBuffer(buffer, node->_value._stringValue);
    node = node->_next;
    if (node) {
      TRI_AppendCharStringBuffer(buffer, '.');
    }
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Hash a member name for comparisons
////////////////////////////////////////////////////////////////////////////////

uint64_t QLAstQueryGetMemberNameHash (TRI_query_node_t* node) {
  TRI_query_node_t *lhs, *rhs;
  uint64_t hashValue;

  lhs = node->_lhs;
  hashValue = TRI_FnvHashString(lhs->_value._stringValue);
  
  rhs = node->_rhs;
  node = rhs->_next;

  while (node) {
    hashValue ^= TRI_FnvHashString(node->_value._stringValue);
    node = node->_next;
  }

  return hashValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

