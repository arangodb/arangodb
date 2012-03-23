////////////////////////////////////////////////////////////////////////////////
/// @brief index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_INDEX_H
#define TRIAGENS_DURHAM_VOC_BASE_INDEX_H 1

#include "VocBase/vocbase.h"

#include "BasicsC/json.h"
#include "ShapedJson/shaped-json.h"
#include "GeoIndex/GeoIndex.h"
#include "HashIndex/hashindex.h"
#include "SkipLists/skiplistIndex.h"
#include "SkipLists/sl-operator.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_collection_s;
struct TRI_doc_mptr_s;
struct TRI_shaped_json_s;
struct TRI_sim_collection_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief index identifier
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t TRI_idx_iid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief index type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_IDX_TYPE_PRIMARY_INDEX,
  TRI_IDX_TYPE_GEO_INDEX,
  TRI_IDX_TYPE_HASH_INDEX,
  TRI_IDX_TYPE_SKIPLIST_INDEX
}
TRI_idx_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index variants
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  INDEX_GEO_NONE = 0,
  INDEX_GEO_INDIVIDUAL_LAT_LON,
  INDEX_GEO_COMBINED_LAT_LON,
  INDEX_GEO_COMBINED_LON_LAT
}
TRI_index_geo_variant_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief index base class
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_s {
  TRI_idx_iid_t _iid;
  TRI_idx_type_e _type;
  struct TRI_doc_collection_s* _collection;

  bool _unique;
  TRI_vector_string_t _fields;

  int (*insert) (struct TRI_index_s*, struct TRI_doc_mptr_s const*);
  int (*remove) (struct TRI_index_s*, struct TRI_doc_mptr_s const*);
  int (*update) (struct TRI_index_s*, struct TRI_doc_mptr_s const*, struct TRI_shaped_json_s const*);
  TRI_json_t* (*json) (struct TRI_index_s*, struct TRI_doc_collection_s*);
}
TRI_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_geo_index_s {
  TRI_index_t base;
  TRI_index_geo_variant_e _variant;

  GeoIndex* _geoIndex;

  TRI_shape_pid_t _location;
  TRI_shape_pid_t _latitude;
  TRI_shape_pid_t _longitude;

  bool _geoJson;
}
TRI_geo_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_hash_index_s {
  TRI_index_t base;

  HashIndex* _hashIndex;  // effectively the associative array
  TRI_vector_t _paths;    // a list of shape pid which identifies the fields of the index
}
TRI_hash_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief skiplist index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_skiplist_index_s {
  TRI_index_t base;

  SkiplistIndex* _skiplistIndex;  // effectively the skiplist
  TRI_vector_t _paths;            // a list of shape pid which identifies the fields of the index
}
TRI_skiplist_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveIndexFile (struct TRI_doc_collection_s* collection, TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_SaveIndex (struct TRI_doc_collection_s*, TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndex (struct TRI_doc_collection_s*, TRI_idx_iid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets name of index type
////////////////////////////////////////////////////////////////////////////////

char const* TRI_TypeNameIndex (const TRI_index_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for lists
///
/// If geoJson is true, than the coordinates should be in the order described
/// in http://geojson.org/geojson-spec.html#positions, which is longitude
/// first and latitude second.
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeoIndex (struct TRI_doc_collection_s*,
                                 char const* locationName,
                                 TRI_shape_pid_t,
                                 bool geoJson);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for arrays
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeoIndex2 (struct TRI_doc_collection_s*,
                                  char const* latitudeName,
                                  TRI_shape_pid_t,
                                  char const* longitudeName,
                                  TRI_shape_pid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyGeoIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeoIndex (TRI_index_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all points within a given radius
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* TRI_WithinGeoIndex (TRI_index_t*,
                                    double lat,
                                    double lon,
                                    double radius);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up the nearest points
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* TRI_NearestGeoIndex (TRI_index_t*,
                                     double lat,
                                     double lon,
                                     size_t count);

                                     
                                     
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash-index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateHashIndex (struct TRI_doc_collection_s*,
                                  TRI_vector_pointer_t* fields,
                                  TRI_vector_t* paths,
                                  bool unique);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashIndex (TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashIndex (TRI_index_t* idx);
                                  
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the hash index
////////////////////////////////////////////////////////////////////////////////

HashIndexElements* TRI_LookupHashIndex (TRI_index_t*, TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

TRI_skiplist_iterator_t* TRI_LookupSkiplistIndex (TRI_index_t*, TRI_sl_operator_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skiplist index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateSkiplistIndex (struct TRI_doc_collection_s*,
                                      TRI_vector_pointer_t* fields,
                                      TRI_vector_t* paths,
                                      bool unique);

                                  
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
