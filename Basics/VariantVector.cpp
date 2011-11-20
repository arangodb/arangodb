////////////////////////////////////////////////////////////////////////////////
/// @brief class for result vectors
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
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "VariantVector.h"

#include <Basics/StringBuffer.h>
#include <Basics/VariantString.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    VariantVector::VariantVector () {
    }



    VariantVector::VariantVector (vector<string> const& elements) {
      for (vector<string>::const_iterator i = elements.begin();  i != elements.end();  ++i) {
        string const& value = *i;
        values.push_back(new VariantString(value));
      }
    }



    VariantVector::~VariantVector () {
      for (vector<VariantObject*>::const_iterator i = values.begin();  i != values.end();  ++i) {
        delete (*i);
      }
    }

    // -----------------------------------------------------------------------------
    // VariantObject methods
    // -----------------------------------------------------------------------------

    VariantObject* VariantVector::clone () const {
      VariantVector* copy = new VariantVector();

      for (vector<VariantObject*>::const_iterator i = values.begin();  i != values.end();  ++i) {
        VariantObject* o = *i;

        copy->add(o->clone());
      }

      return copy;
    }



    void VariantVector::print (StringBuffer& buffer, size_t indent) const {
      buffer.appendText("{\n");

      vector<VariantObject*>::const_iterator j = values.begin();

      for (size_t i = 0;  j != values.end();  ++i, ++j) {
        printIndent(buffer, indent+1);

        buffer.appendInteger(i);
        buffer.appendText(" => ");

        (*j)->print(buffer, indent + 2);
      }

      printIndent(buffer, indent);
      buffer.appendText("}\n");
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void VariantVector::add (VariantObject* value) {
      values.push_back(value);
    }



    void VariantVector::add (vector<VariantObject*> const& v) {
      for (vector<VariantObject*>::const_iterator i = v.begin();  i != v.end();  ++i) {
        VariantObject* value = *i;

        values.push_back(value);
      }
    }



    void VariantVector::add (string const& value) {
      values.push_back(new VariantString(value));
    }



    void VariantVector::add (vector<string> const& v) {
      for (vector<string>::const_iterator i = v.begin();  i != v.end();  ++i) {
        string const& value = *i;

        values.push_back(new VariantString(value));
      }
    }
  }
}

