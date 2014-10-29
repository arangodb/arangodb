////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SHAPED_JSON_JSON__SHAPER_H
#define ARANGODB_SHAPED_JSON_JSON__SHAPER_H 1

#include "Basics/Common.h"

#include "Basics/json.h"
#include "ShapedJson/shaped-json.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       JSON SHAPER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of TRI_shape_path_t
////////////////////////////////////////////////////////////////////////////////

#define TRI_NAME_SHAPE_PATH(path) \
  (((char const*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t))

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a const which represents an undefined weight
///
/// 2^63-1 to indicate that the attribute is weighted to be the lowest possible
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_UNDEFINED_ATTRIBUTE_WEIGHT (-9223372036854775807L)

////////////////////////////////////////////////////////////////////////////////
/// @brief global shape ids and pointers
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_basic_shapes_s {
  TRI_shape_sid_t _sidNull;
  TRI_shape_sid_t _sidBoolean;
  TRI_shape_sid_t _sidNumber;
  TRI_shape_sid_t _sidShortString;
  TRI_shape_sid_t _sidLongString;
  TRI_shape_sid_t _sidList;

  TRI_shape_t     _shapeNull;
  TRI_shape_t     _shapeBoolean;
  TRI_shape_t     _shapeNumber;
  TRI_shape_t     _shapeShortString;
  TRI_shape_t     _shapeLongString;
  TRI_shape_t     _shapeList;
}
TRI_basic_shapes_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shaper_s {
    TRI_shape_aid_t (*findOrCreateAttributeByName) (struct TRI_shaper_s*, char const*);
  TRI_shape_aid_t (*lookupAttributeByName) (struct TRI_shaper_s*, char const*);
  char const* (*lookupAttributeId) (struct TRI_shaper_s*, TRI_shape_aid_t);
  TRI_shape_t const* (*findShape) (struct TRI_shaper_s*, TRI_shape_t*, bool);
  TRI_shape_t const* (*lookupShapeId) (struct TRI_shaper_s*, TRI_shape_sid_t);
  int64_t (*lookupAttributeWeight) (struct TRI_shaper_s*, TRI_shape_aid_t);
  TRI_shape_path_t const* (*lookupAttributePathByPid) (struct TRI_shaper_s*, TRI_shape_pid_t);
  TRI_shape_pid_t (*findOrCreateAttributePathByName) (struct TRI_shaper_s*, char const*);
  TRI_shape_pid_t (*lookupAttributePathByName) (struct TRI_shaper_s*, char const*);

  TRI_associative_synced_t _attributePathsByName;
  TRI_associative_synced_t _attributePathsByPid;

  TRI_shape_pid_t _nextPid;
  TRI_mutex_t _attributePathLock;

  TRI_memory_zone_t* _memoryZone;
}
TRI_shaper_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                            SHAPER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the attribute path
////////////////////////////////////////////////////////////////////////////////

char const* TRI_AttributeNameShapePid (TRI_shaper_t*, TRI_shape_pid_t);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_InitShaper (TRI_shaper_t*, TRI_memory_zone_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShaper (TRI_shaper_t* shaper);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShaper (TRI_shaper_t* shaper);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the sid for a basic type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_sid_t TRI_LookupBasicSidShaper (TRI_shape_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t* TRI_LookupSidBasicShapeShaper (TRI_shape_sid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t* TRI_LookupBasicShapeShaper (TRI_shape_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the first id for user-defined shapes
////////////////////////////////////////////////////////////////////////////////

static inline TRI_shape_sid_t TRI_FirstCustomShapeIdShaper (void) {
  return 7;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises global basic shape types
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseShaper (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown shaper
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownShaper (void);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
