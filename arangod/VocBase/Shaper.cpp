////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Shaper.h"
#include "Basics/associative.h"
#include "Basics/hashes.h"
#include "Basics/StringBuffer.h"
#include "Basics/tri-strings.h"
#include "Basics/vector.h"

// #define DEBUG_JSON_SHAPER 1

TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_ILLEGAL = 0;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_NULL = 1;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_BOOLEAN = 2;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_NUMBER = 3;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_SHORT_STRING = 4;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_LONG_STRING = 5;
TRI_shape_pid_t const BasicShapes::TRI_SHAPE_SID_LIST = 6;

TRI_shape_t const BasicShapes::_shapeNull = {BasicShapes::TRI_SHAPE_SID_NULL,
                                             TRI_SHAPE_NULL,
                                             sizeof(TRI_null_shape_t), 0};

TRI_shape_t const BasicShapes::_shapeBoolean = {
    BasicShapes::TRI_SHAPE_SID_BOOLEAN, TRI_SHAPE_BOOLEAN,
    sizeof(TRI_boolean_shape_t), sizeof(TRI_shape_boolean_t)};

TRI_shape_t const BasicShapes::_shapeNumber = {
    BasicShapes::TRI_SHAPE_SID_NUMBER, TRI_SHAPE_NUMBER,
    sizeof(TRI_number_shape_t), sizeof(TRI_shape_number_t)};

TRI_shape_t const BasicShapes::_shapeShortString = {
    BasicShapes::TRI_SHAPE_SID_SHORT_STRING, TRI_SHAPE_SHORT_STRING,
    sizeof(TRI_short_string_shape_t),
    sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT};

TRI_shape_t const BasicShapes::_shapeLongString = {
    BasicShapes::TRI_SHAPE_SID_LONG_STRING, TRI_SHAPE_LONG_STRING,
    sizeof(TRI_long_string_shape_t), TRI_SHAPE_SIZE_VARIABLE};

TRI_shape_t const BasicShapes::_shapeList = {
    BasicShapes::TRI_SHAPE_SID_LIST, TRI_SHAPE_LIST, sizeof(TRI_list_shape_t),
    TRI_SHAPE_SIZE_VARIABLE};

TRI_shape_t const* BasicShapes::ShapeAddresses[7] = {
    nullptr, &BasicShapes::_shapeNull, &BasicShapes::_shapeBoolean,
    &BasicShapes::_shapeNumber, &BasicShapes::_shapeShortString,
    &BasicShapes::_shapeLongString, &BasicShapes::_shapeList};

////////////////////////////////////////////////////////////////////////////////
/// @brief static const length information for basic shape types
////////////////////////////////////////////////////////////////////////////////

uint32_t const BasicShapes::TypeLengths[5] = {
    0,  // not used
    0,  // null
    sizeof(TRI_shape_boolean_t), sizeof(TRI_shape_number_t),
    sizeof(TRI_shape_length_short_string_t) + TRI_SHAPE_SHORT_STRING_CUT};

static_assert(BasicShapes::TRI_SHAPE_SID_NULL == 1,
              "invalid shape id for null shape");
static_assert(BasicShapes::TRI_SHAPE_SID_BOOLEAN == 2,
              "invalid shape id for boolean shape");
static_assert(BasicShapes::TRI_SHAPE_SID_NUMBER == 3,
              "invalid shape id for number shape");
static_assert(BasicShapes::TRI_SHAPE_SID_SHORT_STRING == 4,
              "invalid shape id for short string shape");

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the shaper
////////////////////////////////////////////////////////////////////////////////

Shaper::Shaper() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the shaper
////////////////////////////////////////////////////////////////////////////////

Shaper::~Shaper() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* Shaper::lookupSidBasicShape(TRI_shape_sid_t sid) {
  if (sid > BasicShapes::TRI_SHAPE_SID_LIST) {
    return nullptr;
  }

  return BasicShapes::ShapeAddresses[sid];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a shape is of primitive type
////////////////////////////////////////////////////////////////////////////////

TRI_shape_t const* Shaper::lookupBasicShape(TRI_shape_t const* shape) {
  if (shape->_type == TRI_SHAPE_NULL) {
    return &BasicShapes::_shapeNull;
  } else if (shape->_type == TRI_SHAPE_BOOLEAN) {
    return &BasicShapes::_shapeBoolean;
  } else if (shape->_type == TRI_SHAPE_NUMBER) {
    return &BasicShapes::_shapeNumber;
  } else if (shape->_type == TRI_SHAPE_SHORT_STRING) {
    return &BasicShapes::_shapeShortString;
  } else if (shape->_type == TRI_SHAPE_LONG_STRING) {
    return &BasicShapes::_shapeLongString;
  } else if (shape->_type == TRI_SHAPE_LIST) {
    return &BasicShapes::_shapeList;
  }

  return nullptr;
}
