////////////////////////////////////////////////////////////////////////////////
/// @brief class for two-dimensional result matrixs
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
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "VariantMatrix2.h"

#include "Basics/StringBuffer.h"
#include "Variant/VariantString.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Variants
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new matrix2
////////////////////////////////////////////////////////////////////////////////

VariantMatrix2::VariantMatrix2 ()
  : _dimensions(), _values() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a new matrix2
////////////////////////////////////////////////////////////////////////////////

VariantMatrix2::~VariantMatrix2 () {
  for (vector< vector<VariantObject*> >::const_iterator i = _values.begin();  i != _values.end();  ++i) {
    vector<VariantObject*> const& line = *i;

    for (vector<VariantObject*>::const_iterator j = line.begin();  j != line.end();  ++j) {
      if (*j != 0) {
        delete *j;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             VariantObject methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Variants
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

VariantObject::ObjectType VariantMatrix2::type () const {
  return TYPE;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

VariantObject* VariantMatrix2::clone () const {
  VariantMatrix2* copy = new VariantMatrix2();

  copy->_dimensions[0] = _dimensions[0];
  copy->_dimensions[1] = _dimensions[1];

  for (vector< vector<VariantObject*> >::const_iterator i = _values.begin();  i != _values.end();  ++i) {
    vector<VariantObject*> const& inner = *i;
    vector<VariantObject*> innerCopy;

    for (vector<VariantObject*>::const_iterator j = inner.begin();  j != inner.end();  ++j) {
      VariantObject* vs = *j;

      innerCopy.push_back(vs->clone());
    }

    copy->_values.push_back(innerCopy);
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void VariantMatrix2::print (StringBuffer& buffer, size_t) const {
  buffer.appendText("(matrix)");
  buffer.appendEol();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Variants
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the dimensions
////////////////////////////////////////////////////////////////////////////////

vector<string> const& VariantMatrix2::getDimension (size_t n) const {
  if (n >= 2) {
    THROW_PARAMETER_ERROR("n", "too large", "getDimension");
  }

  return _dimensions[n];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the values
////////////////////////////////////////////////////////////////////////////////

vector< vector<VariantObject*> > const& VariantMatrix2::getValues () const {
  return _values;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the value at a given position
////////////////////////////////////////////////////////////////////////////////

VariantObject* VariantMatrix2::getValue (size_t x, size_t y) const {
  if (_dimensions[0].size() <= x) {
    THROW_PARAMETER_ERROR("x", "too large", "setValue");
  }

  if (_dimensions[1].size() <= y) {
    THROW_PARAMETER_ERROR("y", "too large", "setValue");
  }

  if (_values.size() <= x) {
    return 0;
  }

  vector<VariantObject*> const& line = _values[x];

  if (line.size() <= y) {
    return 0;
  }

  return line[y];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a value
////////////////////////////////////////////////////////////////////////////////

void VariantMatrix2::setValue (size_t x, size_t y, VariantObject* object) {
  if (_dimensions[0].size() <= x) {
    THROW_PARAMETER_ERROR("x", "too large", "setValue");
  }

  if (_dimensions[1].size() <= y) {
    THROW_PARAMETER_ERROR("y", "too large", "setValue");
  }

  if (_values.size() <= x) {
    _values.resize(x + 1);
  }

  vector<VariantObject*>& line = _values[x];

  if (line.size() <= y) {
    line.resize(y + 1);
  }

  if (line[y] != 0) {
    delete line[y];
  }

  line[y] = object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a dimension element
////////////////////////////////////////////////////////////////////////////////

size_t VariantMatrix2::addDimension (size_t n, string const& name) {
  if (n >= 2) {
    THROW_PARAMETER_ERROR("n", "too large", "addDimension");
  }

  _dimensions[n].push_back(name);

  return _dimensions[n].size() - 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

