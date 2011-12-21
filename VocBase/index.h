////////////////////////////////////////////////////////////////////////////////
/// @brief index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_INDEX_H
#define TRIAGENS_DURHAM_VOC_BASE_INDEX_H 1

#include <VocBase/vocbase.h>

#include <BasicsC/json.h>

#include <GeoIndex/GeoIndex.h>

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
/// @brief index indetifier
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t TRI_idx_iid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief index type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_IDX_TYPE_GEO_INDEX
}
TRI_idx_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief index base class
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_s {
  TRI_idx_iid_t _iid;
  TRI_idx_type_e _type;
  struct TRI_doc_collection_s* _collection;

  bool (*insert) (struct TRI_index_s*, struct TRI_doc_mptr_s const*);
  bool (*remove) (struct TRI_index_s*, struct TRI_doc_mptr_s const*);
  bool (*update) (struct TRI_index_s*, struct TRI_doc_mptr_s const*, struct TRI_shaped_json_s const*);
  TRI_json_t* (*json) (struct TRI_index_s*, struct TRI_doc_collection_s*);
}
TRI_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_geo_index_s {
  TRI_index_t base;

  GeoIndex* _geoIndex;

  TRI_shape_pid_t _location;
  TRI_shape_pid_t _latitude;
  TRI_shape_pid_t _longitude;

  bool _geoJson;
}
TRI_geo_index_t;

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

bool TRI_RemoveIndex (struct TRI_doc_collection_s* collection, TRI_index_t* idx);

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_SaveIndex (struct TRI_doc_collection_s*, TRI_index_t*);

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
                                 TRI_shape_pid_t,
                                 bool geoJson);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for arrays
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeoIndex2 (struct TRI_doc_collection_s*,
                                  TRI_shape_pid_t,
                                  TRI_shape_pid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestoryGeoIndex (TRI_index_t*);

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

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
