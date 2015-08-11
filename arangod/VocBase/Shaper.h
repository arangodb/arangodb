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

#ifndef ARANGODB_VOC_BASE_SHAPER_H
#define ARANGODB_VOC_BASE_SHAPER_H 1

#include "Basics/Common.h"
#include "Basics/json.h"
#include "VocBase/shaped-json.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       BasicShapes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief static const information about basic shape types
////////////////////////////////////////////////////////////////////////////////

struct BasicShapes {
  static TRI_shape_pid_t const   TRI_SHAPE_SID_NULL;
  static TRI_shape_pid_t const   TRI_SHAPE_SID_BOOLEAN;
  static TRI_shape_pid_t const   TRI_SHAPE_SID_NUMBER;
  static TRI_shape_pid_t const   TRI_SHAPE_SID_SHORT_STRING;
  static TRI_shape_pid_t const   TRI_SHAPE_SID_LONG_STRING;
  static TRI_shape_pid_t const   TRI_SHAPE_SID_LIST;

  static TRI_shape_t const       _shapeNull;
  static TRI_shape_t const       _shapeBoolean;
  static TRI_shape_t const       _shapeNumber;
  static TRI_shape_t const       _shapeShortString;
  static TRI_shape_t const       _shapeLongString;
  static TRI_shape_t const       _shapeList;

  static uint32_t const          TypeLengths[5];
  static TRI_shape_t const*      ShapeAddresses[7]; 
};

// -----------------------------------------------------------------------------
// --SECTION--                                                            Shaper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper
////////////////////////////////////////////////////////////////////////////////

class Shaper {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

  public:
    Shaper (Shaper const&) = delete;
    Shaper& operator= (Shaper const&) = delete;
    Shaper ();
    virtual ~Shaper ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape by identifier
////////////////////////////////////////////////////////////////////////////////

    virtual TRI_shape_t const* lookupShapeId (TRI_shape_sid_t) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute name by identifier
////////////////////////////////////////////////////////////////////////////////

    virtual char const* lookupAttributeId (TRI_shape_aid_t) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

    static TRI_shape_t const* lookupSidBasicShape (TRI_shape_sid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

    static TRI_shape_t const* lookupBasicShape (TRI_shape_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the first id for user-defined shapes
////////////////////////////////////////////////////////////////////////////////

    static inline TRI_shape_sid_t firstCustomShapeId () {
      return BasicShapes::TRI_SHAPE_SID_LIST + 1;
    }

};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
