////////////////////////////////////////////////////////////////////////////////
/// @brief shaped JSON objects
///
/// @file
/// Declaration for shaped JSON objects.
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_SHAPED_JSON_SHAPED_JSON_H
#define TRIAGENS_SHAPED_JSON_SHAPED_JSON_H 1

#include "BasicsC/common.h"

#include "BasicsC/json.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page ShapedJson JSON Shapes
///
/// A JSON object is either
///
/// - a scalar type
///  - @c null
///  - a boolean: @c true or @c false
///  - a floating point number
///  - a string
/// - a list (aka array)
/// - a object (aka associative array or hash or document)
///
/// In theory JSON documents are schema-free. They can have any number of
/// attributes and an attribute value can be any JSON object. However, in
/// practice JSON documents often share a common shape. In order to take
/// advantage of this fact, JSON objects can be converted into
/// @ref TRI_shaped_json_t instances together with a shape described by an
/// @ref TRI_shape_t instance.
///
/// A @ref TRI_shape_t describes the layout of the JSON document. Currently,
/// the following shapes are supported:
///
/// - @ref TRI_null_shape_t for the "null" object
/// - @ref TRI_boolean_shape_t for boolean values
/// - @ref TRI_number_shape_t for floating point numbers
/// - @ref TRI_short_string_shape_t for short strings of size less then
///     @ref TRI_SHAPE_SHORT_STRING_CUT, this includes the trailing null
/// - @ref TRI_long_string_shape_t for strings longer than the above limit
/// - @ref TRI_list_shape_t for arbitrary lists
/// - @ref TRI_homogeneous_list_shape_t for lists of objects of the same shape
/// - @ref TRI_homogeneous_sized_list_shape_t for lists of objects of the same
///     shape and size
/// - @ref TRI_array_shape_t for associative arrays
///
/// A shape can be of fixed or arbitrary size. For instance, a
/// @ref TRI_boolean_shape_t is of fixed size, because all boolean JSON
/// objects are of fixed size. The same is true for null, numbers and short
/// strings. A long string is of variable size. Lists are always variable
/// sized. However, a list can be homogeneous or in-homogeneous. A homogeneous
/// list contains only objects of the same shape. For instance, the list
///
///   [ 1, 2, 3, 4, 5 ]
///
/// is homogeneous. In this case it is also homogeneous-sized because all
/// elements of the list have the same size. The list
///
///   [ "Looooooooong String", "Another Looooooooooong String" ]
///
/// is also homogeneous, but not homogeneous-sized as the long strings are
/// variable sized. Note that for a list to be homogeneous-sized the
/// corresponding shape must be fixed sized. Even if in the above example
/// the strings were of equal lengths, that would not imply that the
/// list if homogeneous-sized. It is the shape that matters.
///
/// An array shape can be of fixed or variable size. For instance
///
///  {
///     "a" : 1,
///     "b" : { "c" : 2 }
///   }
///
/// is of fixed size, because all sub-objects are of fixed size. In contrast
/// the object
///
///  {
///     "a" : [ 1 ]
///  }
///
/// is of variable size, because the sub-object stored in "a" is of variable
/// size.
///
/// @section TRI_null_shape_t
///
/// The shape representing the "null" value.
///
/// @copydetails TRI_null_shape_t
///
/// @section TRI_boolean_shape_t
///
/// The shape representing a boolean value.
///
/// @copydetails TRI_boolean_shape_t
///
/// @section TRI_number_shape_t
///
/// The shape representing a number value.
///
/// @copydetails TRI_number_shape_t
///
/// @section TRI_short_string_shape_t
///
/// The shape representing a short string.
///
/// @copydetails TRI_short_string_shape_t
///
/// @section TRI_long_string_shape_t
///
/// The shape representing a string.
///
/// @copydetails TRI_long_string_shape_t
///
/// @section TRI_array_shape_t
///
/// The most complex shape is the shape of an associative array.
///
/// @copydetails TRI_array_shape_t
///
/// @section TRI_list_shape_t
///
/// The shape representing a list.
///
/// @copydetails TRI_list_shape_t
///
/// @section TRI_homogeneous_list_shape_t
///
/// The shape representing a homogeneous list.
///
/// @copydetails TRI_homogeneous_list_shape_t
///
/// @section TRI_homogeneous_sized_list_shape_t
///
/// The shape representing a homogeneous list.
///
/// @copydetails TRI_homogeneous_sized_list_shape_t
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_shaper_s;
struct TRI_string_buffer_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief size of short strings
////////////////////////////////////////////////////////////////////////////////

#define TRI_SHAPE_SHORT_STRING_CUT 7

////////////////////////////////////////////////////////////////////////////////
/// @brief indicator for variable sized data
////////////////////////////////////////////////////////////////////////////////

#define TRI_SHAPE_SIZE_VARIABLE ((TRI_shape_size_t) -1)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        JSON SHAPE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a shape identifier
///
/// @note 0 is not a valid shape identifier.
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_shape_sid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of an attribute identifier
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_shape_aid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a size
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_shape_size_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json type of a shape
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_SHAPE_ILLEGAL = 0,
  TRI_SHAPE_NULL = 1,
  TRI_SHAPE_BOOLEAN = 2,
  TRI_SHAPE_NUMBER = 3,
  TRI_SHAPE_SHORT_STRING = 4,
  TRI_SHAPE_LONG_STRING = 5,
  TRI_SHAPE_ARRAY = 6,
  TRI_SHAPE_LIST = 7,
  TRI_SHAPE_HOMOGENEOUS_LIST = 8,
  TRI_SHAPE_HOMOGENEOUS_SIZED_LIST = 9
}
TRI_shape_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a TRI_shape_type_e
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_shape_type_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a boolean
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_shape_boolean_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a number
////////////////////////////////////////////////////////////////////////////////

typedef double TRI_shape_number_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a length for short strings
////////////////////////////////////////////////////////////////////////////////

typedef uint8_t TRI_shape_length_short_string_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a length for long strings
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_shape_length_long_string_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of a length for lists
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_shape_length_list_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for json shape
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shape_s {
  TRI_shape_sid_t _sid;
  TRI_shape_type_t _type;
  TRI_shape_size_t _size; // total size of the shape
  TRI_shape_size_t _dataSize; // in case of fixed sized shaped or TRI_SHAPE_SIZE_VARIABLE
}
TRI_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief entry/value structure
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shape_value_s {
  TRI_shape_aid_t _aid;        // attribute identifier

  TRI_shape_sid_t _sid;        // shape identifier of the attribute
  TRI_shape_type_t _type;      // type of the attribute
  bool _fixedSized;            // true of all element of this shaped have the same size
  TRI_shape_size_t _size;      // size of the data block

  char* _value;
}
TRI_shape_value_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, null
///
/// A @c TRI_null_shape_t describes the null value. There are no additional
/// attributes.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_NULL</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_null_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always 0</td>
///   </tr>
/// </table>
///
/// There is no corresponding shaped JSON object.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_null_shape_s {
  TRI_shape_t base;
}
TRI_null_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, boolean
///
/// A @c TRI_boolean_shape_t describes a boolean value. There are no additional
/// attributes.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_BOOLEAN</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_boolean_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always sizeof(TRI_shape_boolean_t)</td>
///   </tr>
/// </table>
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_boolean_t</td>
///     <td>_value</td>
///     <td>the boolean value: 0 or 1</td>
///   </tr>
/// </table>
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_boolean_shape_s {
  TRI_shape_t base;
}
TRI_boolean_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, number
///
/// A @c TRI_number_shape_t describes a number value. There are no additional
/// attributes.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_NUMBER</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_number_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always sizeof(TRI_shape_number_t)</td>
///   </tr>
/// </table>
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_number_t</td>
///     <td>_value</td>
///     <td>the number value</td>
///   </tr>
/// </table>
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_number_shape_s {
  TRI_shape_t base;
}
TRI_number_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, short string
///
/// A @c TRI_short_string_shape_t describes a short string value. There are no
/// additional attributes.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_SHORT_STRING</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_short_string_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always sizeof(TRI_shape_length_short_string_t) + @c TRI_SHAPE_SHORT_STRING_CUT</td>
///   </tr>
/// </table>
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_length_short_string_t</td>
///     <td>_length</td>
///     <td>the length of the string including the final '\0'</td>
///   </tr>
///   <tr>
///     <td>@c char</td>
///     <td>_value[TRI_SHAPE_SHORT_STRING_CUT]</td>
///     <td>the string</td>
///   </tr>
/// </table>
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_short_string_shape_s {
  TRI_shape_t base;
}
TRI_short_string_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, long string
///
/// A @c TRI_long_string_shape_t describes a string value. There are no
/// additional attributes.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_LONG_STRING</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_long_string_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always @c TRI_SHAPE_SIZE_VARIABLE</td>
///   </tr>
/// </table>
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_length_long_string_t</td>
///     <td>_length</td>
///     <td>the length of the string including the final '\0'</td>
///   </tr>
///   <tr>
///     <td>@c char</td>
///     <td>_value[_length]</td>
///     <td>the string</td>
///   </tr>
/// </table>
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_long_string_shape_s {
  TRI_shape_t base;
}
TRI_long_string_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, array
///
/// A @c TRI_array_shape_t describes a JSON associative array. Such an array
/// consists of keys (aka attributes) and values. The values themself are
/// again described by @c TRI_shape_t instances. The keys are divided into
/// keys belonging to fixed sized values and into keys belonging to variable
/// sized values. The offsets into the data array of an @c TRI_shaped_json_t
/// for the fixed sized values are stored inside the shape. The offsets for the
/// variable sized values are store inside the shaped JSON. The memory layout
/// of a array shape is as follows:
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_ARRAY</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>eitder @c TRI_SHAPE_SIZE_VARIABLE or tde size of tde data for fixed sized arrays</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_fixedEntries</td>
///     <td>number of fixed attributes</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_variableEntries</td>
///     <td>number of variable attributes</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sids[_fixedEntries + _variableEntries]</td>
///     <td>shape identifier of the corresponding attribute</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_aid_t</td>
///     <td>_aids[_fixedEntries + _variableEntries]</td>
///     <td>attribute identifier of the corresponding attribute</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_offsets[_fixedEntries + 1]</td>
///     <td>offsets into the @c TRI_shaped_json_t object</td>
///   </tr>
/// </table>
///
/// Note that the data block for the n.the fixed sized attribute is between
/// offset _offsets[n] (inclusive) and _offsets[n + 1] (exclusive).
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_offsets[_variableEntries + 1]</td>
///     <td>offsets into myself</td>
///   </tr>
///   <tr>
///     <td>byte[size1]</td>
///     <td>_attribute1</td>
///     <td>memory block of the value of the first attribute</td>
///   </tr>
///   <tr>
///     <td>byte[size2]</td>
///     <td>_attribute2</td>
///     <td>memory block of the value of the second attribute</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>...</td>
///     <td>...</td>
///   </tr>
/// </table>
///
/// The first variable sized attributes is stored between offset _offsets[0]
/// (inclusive) and _offsets[1] (exclusive).
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_array_shape_s {
  TRI_shape_t base;

  TRI_shape_size_t _fixedEntries;
  TRI_shape_size_t _variableEntries;

  // TRI_shape_sid_t _sids[_fixedEntries + _variableEntries]
  // TRI_shape_aid_t _aids[_fixedEntries + _variableEntries]
  // TRI_shape_size_t _offsets[_fixedEntries + 1]
}
TRI_array_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, in-homogeneous list
///
/// A @c TRI_list_shape_t describes a list value. There are no additional
/// attributes.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_LIST</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_list_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always @c TRI_SHAPE_SIZE_VARIABLE</td>
///   </tr>
/// </table>
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_length_list_t</td>
///     <td>_length</td>
///     <td>the length of the list</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sids[_length]</td>
///     <td>the shape identifiers for the list entries</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_offsets[_length + 1]</td>
///     <td>offsets into the @c TRI_shaped_json_t object</td>
///   </tr>
///   <tr>
///     <td>byte[size1]</td>
///     <td>_attribute1</td>
///     <td>memory block of the value of the first attribute</td>
///   </tr>
///   <tr>
///     <td>byte[size2]</td>
///     <td>_attribute2</td>
///     <td>memory block of the value of the second attribute</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>...</td>
///     <td>...</td>
///   </tr>
/// </table>
///
/// The first variable list element is stored between offset _offsets[0]
/// (inclusive) and _offsets[1] (exclusive).
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_list_shape_s {
  TRI_shape_t base;
}
TRI_list_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, homogeneous list, in-homogeneous size
///
/// A @c TRI_homogeneous_list_shape_t describes a homogeneous list value. That is
/// to say, all list elements have the same shape, but possible different sizes.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_LIST</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_list_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always @c TRI_SHAPE_SIZE_VARIABLE</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sidEntry</td>
///     <td>the shape identifier of all list entries</td>
///   </tr>
/// </table>
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_length_list_t</td>
///     <td>_length</td>
///     <td>the length of the list</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_offsets[_length + 1]</td>
///     <td>offsets into the @c TRI_shaped_json_t object</td>
///   </tr>
///   <tr>
///     <td>byte[size1]</td>
///     <td>_attribute1</td>
///     <td>memory block of the value of the first attribute</td>
///   </tr>
///   <tr>
///     <td>byte[size2]</td>
///     <td>_attribute2</td>
///     <td>memory block of the value of the second attribute</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>...</td>
///     <td>...</td>
///   </tr>
/// </table>
///
/// The first variable list element is stored between offset _offsets[0]
/// (inclusive) and _offsets[1] (exclusive).
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_homogeneous_list_shape_s {
  TRI_shape_t base;

  TRI_shape_sid_t _sidEntry;
}
TRI_homogeneous_list_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json shape, homogeneous list, homogeneous size
///
/// A @c TRI_homogeneous_sized_list_shape_t describes a homogeneous, equally
/// sized list value. That is to say, all list elements have the same shape and
/// the same size.
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sid</td>
///     <td>shape identifier</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_type_t</td>
///     <td>_type</td>
///     <td>always @c TRI_SHAPE_LIST</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_size</td>
///     <td>total size of the shape, always sizeof(TRI_list_shape_t)</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_dataSize</td>
///     <td>always @c TRI_SHAPE_SIZE_VARIABLE</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_sid_t</td>
///     <td>_sidEntry</td>
///     <td>the shape identifier of all list entries</td>
///   </tr>
///   <tr>
///     <td>@c TRI_shape_size_t</td>
///     <td>_sizeEntry</td>
///     <td>the common size of all list entries</td>
///   </tr>
/// </table>
///
/// The memory layout of the corresponding shaped JSON is as follows
///
/// <table border>
///   <tr>
///     <td>@c TRI_shape_length_list_t</td>
///     <td>_length</td>
///     <td>the length of the list</td>
///   </tr>
///   <tr>
///     <td>byte[size1]</td>
///     <td>_attribute1</td>
///     <td>memory block of the value of the first attribute</td>
///   </tr>
///   <tr>
///     <td>byte[size2]</td>
///     <td>_attribute2</td>
///     <td>memory block of the value of the second attribute</td>
///   </tr>
///   <tr>
///     <td>...</td>
///     <td>...</td>
///     <td>...</td>
///   </tr>
/// </table>
///
/// The first variable list element is stored between offset 0 (inclusive) and
/// _sizeEntry (exclusive).
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_homogeneous_sized_list_shape_s {
  TRI_shape_t base;

  TRI_shape_sid_t _sidEntry;
  TRI_shape_size_t _sizeEntry;
}
TRI_homogeneous_sized_list_shape_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief shaped json
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shaped_json_s {
  TRI_shape_sid_t _sid;
  TRI_blob_t _data;
}
TRI_shaped_json_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief shaped json sub-object
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shaped_sub_s {
  TRI_shape_sid_t _sid;
  size_t _offset;
  size_t _length;
}
TRI_shaped_sub_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    ATTRIBUTE PATH
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief json storage type of an attribute path
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_shape_pid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief json attribute path
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_shape_path_s {
  TRI_shape_pid_t _pid;
  uint64_t _aidLength;
  uint32_t _nameLength;  // include trailing '\0'
  // TRI_shape_aid_t _aids[];
  // char _name[];
}
TRI_shape_path_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a deep copy of a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_CopyShapedJson (struct TRI_shaper_s*, TRI_shaped_json_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShapedJson (struct TRI_shaper_s*, TRI_shaped_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a json object and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapedJson (struct TRI_shaper_s*, TRI_shaped_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a list of TRI_shape_value_t
////////////////////////////////////////////////////////////////////////////////

void TRI_SortShapeValues (TRI_shape_value_t* values, size_t n);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a json object into a shaped json object
////////////////////////////////////////////////////////////////////////////////

TRI_shaped_json_t* TRI_ShapedJsonJson (struct TRI_shaper_s*, TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a shaped json object into a json object
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonShapedJson (struct TRI_shaper_s*, TRI_shaped_json_t const*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer, without the outer braces
/// this can only be used to stringify shapes of type array
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyArrayShapedJson (struct TRI_shaper_s*,
                                   struct TRI_string_buffer_s*,
                                   TRI_shaped_json_t const*,
                                   bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyShapedJson (struct TRI_shaper_s* shaper,
                              struct TRI_string_buffer_s*,
                              TRI_shaped_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a shaped json to a string buffer
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringifyAugmentedShapedJson (struct TRI_shaper_s* shaper,
                                       struct TRI_string_buffer_s*,
                                       TRI_shaped_json_t const*,
                                       TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthListShapedJson (TRI_list_shape_t const*,
                                 TRI_shaped_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtListShapedJson (TRI_list_shape_t const* shape,
                           TRI_shaped_json_t const* json,
                           size_t position,
                           TRI_shaped_json_t* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a homogeneous list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthHomogeneousListShapedJson (TRI_homogeneous_list_shape_t const* shape,
                                            TRI_shaped_json_t const* json);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a homogeneous list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtHomogeneousListShapedJson (TRI_homogeneous_list_shape_t const* shape,
                                      TRI_shaped_json_t const* json,
                                      size_t position,
                                      TRI_shaped_json_t* result);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of a homogeneous sized list
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthHomogeneousSizedListShapedJson (TRI_homogeneous_sized_list_shape_t const* shape,
                                                 TRI_shaped_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the n.th element of a homogeneous sized list
////////////////////////////////////////////////////////////////////////////////

bool TRI_AtHomogeneousSizedListShapedJson (TRI_homogeneous_sized_list_shape_t const* shape,
                                           TRI_shaped_json_t const*,
                                           size_t position,
                                           TRI_shaped_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a TRI_shape_t for debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_PrintShape (struct TRI_shaper_s* shaper, TRI_shape_t const* shape, int indent);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string value encoded in a shaped json
/// this will return the pointer to the string and the string length in the
/// variables passed by reference
////////////////////////////////////////////////////////////////////////////////

bool TRI_StringValueShapedJson (const TRI_shape_t* const,
                                const TRI_shaped_json_t* const,
                                char**,
                                size_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
