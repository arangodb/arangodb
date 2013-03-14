////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for result objects
///
/// @file
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
/// @author Achim Brandt
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "VariantObject.h"

#include "Basics/StringBuffer.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Variants
/// @{
////////////////////////////////////////////////////////////////////////////////

string VariantObject::NameObjectType (ObjectType type) {
  switch (type) {
    case VARIANT_ARRAY: return "ARRAY";
    case VARIANT_BLOB: return "BLOB";
    case VARIANT_BOOLEAN: return "BOOLEAN";
    case VARIANT_DATE: return "DATE";
    case VARIANT_DATETIME: return "DATETIME";
    case VARIANT_DOUBLE: return "DOUBLE";
    case VARIANT_FLOAT: return "FLOAT";
    case VARIANT_INT8: return "INT8";
    case VARIANT_INT16: return "INT16";
    case VARIANT_INT32: return "INT32";
    case VARIANT_INT64: return "INT64";
    case VARIANT_MATRIX2: return "MATRIX2";
    case VARIANT_NULL: return "NULL";
    case VARIANT_ROW: return "ROW";
    case VARIANT_STRING: return "STRING";
    case VARIANT_UINT8: return "UINT8";
    case VARIANT_UINT16: return "UINT16";
    case VARIANT_UINT32: return "UINT32";
    case VARIANT_UINT64: return "UINT64";
    case VARIANT_VECTOR: return "VECTOR";
  }
  return "UNKNOWN";
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Variants
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a result object
////////////////////////////////////////////////////////////////////////////////

VariantObject::VariantObject () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a result object
////////////////////////////////////////////////////////////////////////////////

VariantObject::~VariantObject () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Variants
/// @{
////////////////////////////////////////////////////////////////////////////////

void VariantObject::printIndent (StringBuffer& buffer, size_t indent) const {
  for (size_t i = 0;  i < indent;  ++i) {
    buffer.appendText("  ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
