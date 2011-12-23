////////////////////////////////////////////////////////////////////////////////
/// @brief class for two-dimensional result matrixs
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "VariantMatrix2.h"

#include <Basics/StringBuffer.h>
#include <Variant/VariantString.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    VariantMatrix2::VariantMatrix2 () {
    }



    VariantMatrix2::~VariantMatrix2 () {
      for (vector< vector<VariantObject*> >::const_iterator i = values.begin();  i != values.end();  ++i) {
        vector<VariantObject*> const& line = *i;

        for (vector<VariantObject*>::const_iterator j = line.begin();  j != line.end();  ++j) {
          if (*j != 0) {
            delete *j;
          }
        }
      }
    }

    // -----------------------------------------------------------------------------
    // VariantObject methods
    // -----------------------------------------------------------------------------

    VariantObject* VariantMatrix2::clone () const {
      VariantMatrix2* copy = new VariantMatrix2();

      copy->dimensions[0] = dimensions[0];
      copy->dimensions[1] = dimensions[1];

      for (vector< vector<VariantObject*> >::const_iterator i = values.begin();  i != values.end();  ++i) {
        vector<VariantObject*> const& inner = *i;
        vector<VariantObject*> innerCopy;

        for (vector<VariantObject*>::const_iterator j = inner.begin();  j != inner.end();  ++j) {
          VariantObject* vs = *j;

          innerCopy.push_back(vs->clone());
        }

        copy->values.push_back(innerCopy);
      }

      return copy;
    }



    void VariantMatrix2::print (StringBuffer& buffer, size_t) const {
      buffer.appendText("(matrix)");
      buffer.appendEol();
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    vector<string> const& VariantMatrix2::getDimension (size_t n) const {
      if (n >= 2) {
        THROW_PARAMETER_ERROR("n", "getDimension", "too large");
      }

      return dimensions[n];
    }



    VariantObject* VariantMatrix2::getValue (size_t x, size_t y) const {
      if (dimensions[0].size() <= x) {
        THROW_PARAMETER_ERROR("x", "setValue", "too large");
      }

      if (dimensions[1].size() <= y) {
        THROW_PARAMETER_ERROR("y", "setValue", "too large");
      }

      if (values.size() <= x) {
        return 0;
      }

      vector<VariantObject*> const& line = values[x];

      if (line.size() <= y) {
        return 0;
      }

      return line[y];
    }


    void VariantMatrix2::setValue (size_t x, size_t y, VariantObject* object) {
      if (dimensions[0].size() <= x) {
        THROW_PARAMETER_ERROR("x", "setValue", "too large");
      }

      if (dimensions[1].size() <= y) {
        THROW_PARAMETER_ERROR("y", "setValue", "too large");
      }

      if (values.size() <= x) {
        values.resize(x + 1);
      }

      vector<VariantObject*>& line = values[x];

      if (line.size() <= y) {
        line.resize(y + 1);
      }

      if (line[y] != 0) {
        delete line[y];
      }

      line[y] = object;
    }



    size_t VariantMatrix2::addDimension (size_t n, string const& name) {
      if (n >= 2) {
        THROW_PARAMETER_ERROR("n", "addDimension", "too large");
      }

      dimensions[n].push_back(name);

      return dimensions[n].size() - 1;
    }
  }
}
