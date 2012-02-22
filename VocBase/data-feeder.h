////////////////////////////////////////////////////////////////////////////////
/// @brief data feeders for selects
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

#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/json.h>

#include "VocBase/simple-collection.h"
#include "VocBase/result.h"
#include "VocBase/context.h"
#include "QL/optimize.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page indexusage Index usage
/// 
/// When executing a query, the database will automatically check if it can use 
/// an index to speed up the query. It will check all available indexes for the
/// collections used in the query and will picks the ones that are (most) 
/// appropriate. This process is called index selection.
///
/// The index selection is done for each collection used in a query. If a 
/// collection is used multiple times in a query (e.g. 
/// @LIT{users u1 INNER JOIN users u2 ON (u1.id == u2.id)}, then there will be 
/// a separate index selection per collection instance.
///
/// @section Requirements
///
/// Which index is used depends on which indexes are available for the collections
/// used and what is contained in the query's WHERE and JOIN conditions.
///
/// An index can only be used if the WHERE/JOIN conditions refer to indexed 
/// attributes. It depends on the index type what kinds of comparisons are allowed
/// in order to use the index. It also depends on the index type whether just a
/// subset of the indexed attributes is sufficient in order to use an index.
/// 
/// There are the following index types:
/// - primary index (automatically created for the "_id" attribute of a collection)
/// - hash index (used-defined index on one or many attributes of a collection)
/// - geo index (user-defined index on two attributes of a collection)
///
/// @subsection Primary index
///
/// The collection's primary index will only be used to access the documents of a
/// collection if the WHERE/JOIN condition for the collection contains an equality
/// predicate for the @LIT{_id} attribute. The compare value must either be a 
/// string constant (e.g. @LIT{u._id == "345055525:346693925"} or a reference to 
/// another attribute (e.g. @LIT{u._id == x.value}.
///
/// A collection's primary index will not be used for any comparison other than
/// equality comparisons or for multi-attribute predicates.
///
/// @subsection Hash index
///
/// Hash indexes for collections can be used if all of the indexed attributes are
/// specified in the WHERE/JOIN condition. It is not sufficient to use just a subset
/// of the indexed attributes in a query. The condition for each attribute must 
/// also be an equality predicate. The compare value must be a string or numeric
/// constant or a reference to another attribute.
///
/// Provided there is an index on @LIT{u.first} and @LIT{u.last}, the index could
/// be used for the following predicates:
/// - @LIT{u.first == 'Jack' && u.last == 'Sparrow'}
/// - @LIT{u.last == 'Sparrow' && u.first == 'Jack'}
/// 
/// A hash index will not be used for any comparison other than equality comparsions
/// or for conditions that do not contain all indexed attributes.
///
/// @section Index preference
/// 
/// As mentioned before, The index selection process will pick the most appropriate
/// index for each collection. The definition of "appropriate" in this context is:
///
/// - If the primary index can be used, it will be used. The reason for this is that
///   the primary index is unique and guaranteed to return at most one document.
///   Furthermore, the primary index is present in memory anyway and access to it is
///   fast.
/// - If the primary index cannot be used, all candidate hash indexes will be
///   checked. If there are multiple candidate, the hash index with the most 
///   attributes indexes is picked. The assumption behind this is that the more 
///   attributes are indexed, the less selective the index is expected to be and
///   the less documents it is supposed to return for each compare value. If there 
///   is only one candidate hash index, it will be used.
/// - If no index can be used to access the documents in a collection, a full 
///   collection scan will be done. 
///
/// There is no way to explicitly specify which index to use/prefer/reject in a
/// query as there sometimes is in other database products.
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief forward declaration to resolve header inclusion issues
////////////////////////////////////////////////////////////////////////////////
                                                      
typedef void TRI_join_t;

// -----------------------------------------------------------------------------
// --SECTION--                                         general feeder attributes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief data feeder access types
///
/// - ACCESS_ALL: full table scan, no index used
/// - ACCESS_CONST: index usage, index is queried with const value(s)
/// - ACCESS_REF: index usage, index is queried with values from other tables
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  ACCESS_ALL    = 1,
  ACCESS_CONST  = 1,
  ACCESS_REF    = 2
}
TRI_index_access_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief data feeder types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  FEEDER_TABLE_SCAN      = 1,
  FEEDER_PRIMARY_LOOKUP  = 2,
  FEEDER_HASH_LOOKUP     = 3
}
TRI_data_feeder_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief general data feeder interface (used by all variants)
///
/// A data feeder is a means of accessing the documents in a collection in a
/// select query.
///
/// For each collection in a query, one data feeder will be used. If a collection 
/// is invoked multiple times in a select (e.g. A INNER JOIN A) then there will
/// be multiple data feeders (in this case for collection A). This is because
/// the data feeder also contains state information (current position) that is
/// distinct for multiple instances of one collection in the same join.
///
/// The data feeder's internal state depends on the data feeder type (@ref
/// TRI_data_feeder_type_e). 
///
/// Index-based data feeders might access the index values using constants or
/// references to other fields. Using constants (e.g. a.id == 5) is of course
/// the fastest way because the compare value is constant for the complete join 
/// process. The compare value can be set up once at the start and will simply
/// be reused. 
/// If the compare value is not constant but a reference to another field 
/// (e.g. a.id == b.id), then the compare value is dynamic and will be determined
/// by a Javascript function for each iteration. The Javascript function is
/// set up once only.
///
/// Data feeders are first initialized by calling their init() function. This 
/// function must set up all internal structures. Const access data feeders
/// can initialize their compare value(s) with the constants here already so 
/// they do not need to be initialized in each join comparison. Ref access data
/// feeders can initialize their Javascript function here.
///
/// The rewind() function will be called at the start of the join execution to
/// reset the data feeder position to the beginning of the data. The rewind
/// function is called multiple times for inner collections in a join (once for
/// each combination of documents in outer scope).
///
/// The current() function is called during join execution to return the current
/// document. It might return a nil pointer if there are no more documents.
/// The current() function is expected to move the position pointer forward by
/// one document.
///
/// The eof() function is used to check if there are more documents available.
///
/// The free() function is finally called after join processing is done and is
/// expected to free all internal structures.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_data_feeder_s {
  TRI_data_feeder_type_e _type;
  TRI_index_access_type_e _accessType;
  TRI_idx_iid_t _indexId;
  TRI_vector_pointer_t* _ranges;
  TRI_join_t* _join;
  size_t _level;
  void* _state;
  const TRI_doc_collection_t* _collection;
  
  void (*init) (struct TRI_data_feeder_s*);
  void (*rewind) (struct TRI_data_feeder_s*);
  TRI_doc_mptr_t* (*current) (struct TRI_data_feeder_s*);
  bool (*eof) (struct TRI_data_feeder_s*);
  void (*free) (struct TRI_data_feeder_s*);
}
TRI_data_feeder_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                        table scan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief internals/guts of table scan data feeder
///
/// A table scanner is used to access the documents in a collection sequentially.
/// The documents are accessed in order of definition in the collection's hash
/// table. The hash table might also contain empty entries (nil pointers) or
/// deleted documents. The data feeder abstracts all this and provides easy 
/// access to all (relevant) documents in the hash table. 
///
/// The table scanner does not have any other internal state than positioning
/// information. As it will return all documents anyway, it does not have any
/// distinction between const and ref access types.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_data_feeder_table_scan_s {
  void **_start;
  void **_end;
  void **_current;
}
TRI_data_feeder_table_scan_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new data feeder (table scan)
////////////////////////////////////////////////////////////////////////////////

TRI_data_feeder_t* TRI_CreateDataFeederTableScan (const TRI_doc_collection_t*,
                                                  TRI_join_t*,
                                                  size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                     primary index
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief internals/guts of primary index data feeder
/// 
/// The primary index data feeder will always use the (unique) primary index of
/// a collection to find exactly one (or zero) documents. It supports const and
/// ref access.  
////////////////////////////////////////////////////////////////////////////////
  
typedef struct TRI_data_feeder_primary_lookup_s {
  bool _hasCompared; 
  bool _isEmpty;
  TRI_voc_did_t _didValue;
  TRI_js_exec_context_t _context;
}
TRI_data_feeder_primary_lookup_t;


////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new data feeder (primary index lookup)
////////////////////////////////////////////////////////////////////////////////

TRI_data_feeder_t* TRI_CreateDataFeederPrimaryLookup (const TRI_doc_collection_t*,
                                                      TRI_join_t*,
                                                      size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                        hash index
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief internals/guts of hash lookup data feeder
///
/// The hash index data feeder will use a unique or non-unique hash index 
/// defined for a collection. It will return any documents available in the 
/// for the compare values. It supports const and ref access.  
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_data_feeder_hash_lookup_s {
  bool _isEmpty;
  TRI_index_t* _index;
  HashIndexElements* _hashElements;
  TRI_js_exec_context_t _context;
  size_t _position;
}
TRI_data_feeder_hash_lookup_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a new data feeder (hash index lookup)
////////////////////////////////////////////////////////////////////////////////

TRI_data_feeder_t* TRI_CreateDataFeederHashLookup (const TRI_doc_collection_t*,
                                                   TRI_join_t*,
                                                   size_t);

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

