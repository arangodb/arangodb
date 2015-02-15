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
/// @brief static const information about basic shape types
////////////////////////////////////////////////////////////////////////////////

struct BasicShapes {
  static TRI_shape_pid_t const TRI_SHAPE_SID_NULL;
  static TRI_shape_pid_t const TRI_SHAPE_SID_BOOLEAN;
  static TRI_shape_pid_t const TRI_SHAPE_SID_NUMBER;
  static TRI_shape_pid_t const TRI_SHAPE_SID_SHORT_STRING;
  static TRI_shape_pid_t const TRI_SHAPE_SID_LONG_STRING;
  static TRI_shape_pid_t const TRI_SHAPE_SID_LIST;

  static TRI_shape_t const     _shapeNull;
  static TRI_shape_t const     _shapeBoolean;
  static TRI_shape_t const     _shapeNumber;
  static TRI_shape_t const     _shapeShortString;
  static TRI_shape_t const     _shapeLongString;
  static TRI_shape_t const     _shapeList;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shaper_s {
  TRI_shape_aid_t (*findOrCreateAttributeByName) (struct TRI_shaper_s*, char const*);
  TRI_shape_aid_t (*lookupAttributeByName) (struct TRI_shaper_s*, char const*);
  char const* (*lookupAttributeId) (struct TRI_shaper_s*, TRI_shape_aid_t);
  TRI_shape_t const* (*findShape) (struct TRI_shaper_s*, TRI_shape_t*, bool);
  TRI_shape_t const* (*lookupShapeId) (struct TRI_shaper_s*, TRI_shape_sid_t);
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
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* TRI_LookupSidBasicShapeShaper (TRI_shape_sid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* TRI_LookupBasicShapeShaper (TRI_shape_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the first id for user-defined shapes
////////////////////////////////////////////////////////////////////////////////

static inline TRI_shape_sid_t TRI_FirstCustomShapeIdShaper (void) {
  return BasicShapes::TRI_SHAPE_SID_LIST + 1;
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
