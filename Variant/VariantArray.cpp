////////////////////////////////////////////////////////////////////////////////
/// @brief class for result arrays
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

#include "VariantArray.h"

#include <Basics/StringBuffer.h>
#include <Variant/VariantBlob.h>
#include <Variant/VariantBoolean.h>
#include <Variant/VariantDate.h>
#include <Variant/VariantDatetime.h>
#include <Variant/VariantDouble.h>
#include <Variant/VariantFloat.h>
#include <Variant/VariantInt8.h>
#include <Variant/VariantInt16.h>
#include <Variant/VariantInt32.h>
#include <Variant/VariantInt64.h>
#include <Variant/VariantMatrix2.h>
#include <Variant/VariantNull.h>
#include <Variant/VariantString.h>
#include <Variant/VariantUInt8.h>
#include <Variant/VariantUInt16.h>
#include <Variant/VariantUInt32.h>
#include <Variant/VariantUInt64.h>
#include <Variant/VariantVector.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    VariantArray::VariantArray ()
      : attributes(), mapping(), nextKey() {
    }



    VariantArray::~VariantArray () {
      for (map<string, VariantObject*>::const_iterator i = mapping.begin();  i != mapping.end();  ++i) {
        VariantObject* value = i->second;

        if (value != 0) {
          delete value;
        }
      }
    }

    // -----------------------------------------------------------------------------
    // VariantObject methods
    // -----------------------------------------------------------------------------

    VariantObject* VariantArray::clone () const {
      VariantArray* copy = new VariantArray();

      for (map<string, VariantObject*>::const_iterator i = mapping.begin();  i != mapping.end();  ++i) {
        copy->add(i->first, i->second->clone());
      }

      return copy;
    }



    void VariantArray::print (StringBuffer& buffer, size_t indent) const {
      buffer.appendText("{\n");

      for (map<string, VariantObject*>::const_iterator i = mapping.begin();  i != mapping.end();  ++i) {
        printIndent(buffer, indent + 1);

        buffer.appendText(i->first);
        buffer.appendText(" => ");

        i->second->print(buffer, indent + 2);
      }

      printIndent(buffer, indent);
      buffer.appendText("}\n");
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    vector<VariantObject*> VariantArray::getValues () const {
      vector<VariantObject*> result;

      for (vector<string>::const_iterator i = attributes.begin();  i != attributes.end();  ++i) {
        string const& key = *i;
        map<string, VariantObject*>::const_iterator j = mapping.find(key);

        if (j == mapping.end()) {
          result.push_back(0);
        }
        else {
          result.push_back(j->second);
        }
      }

      return result;
    }



    void VariantArray::add (string const& key, VariantObject* value) {
      map<string, VariantObject*>::const_iterator j = mapping.find(key);

      if (j == mapping.end()) {
        attributes.push_back(key);
        mapping[key] = value;
      }
      else {
        delete j->second;
        mapping[key] = value;
      }
    }



    void VariantArray::add (string const& key, string const& valueString) {
      add(key, new VariantString(valueString));
    }



    void VariantArray::add (vector<string> const& kv) {
      for (vector<string>::const_iterator i = kv.begin();  i != kv.end();  i += 2) {
        string const& key = *i;

        add(key, new VariantString(*(i+1)));
      }
    }



    void VariantArray::addNextKey (string const& key) {
      nextKey = key;
    }



    void VariantArray::addNextValue (VariantObject* value) {
      if (! nextKey.empty()) {
        add(nextKey, value);
        nextKey = "";
      }
      else {
        delete value;
      }
    }



    VariantObject* VariantArray::lookup (string const& name) {
      map<string,VariantObject*>::iterator i = mapping.find(name);

      if (i == mapping.end()) {
        return 0;
      }
      else {
        return i->second;
      }
    }




    VariantArray* VariantArray::lookupArray (string const& name) {
      return dynamic_cast<VariantArray*>(lookup(name));
    }




    VariantBlob* VariantArray::lookupBlob (string const& name) {
      return dynamic_cast<VariantBlob*>(lookup(name));
    }




    VariantBoolean* VariantArray::lookupBoolean (string const& name) {
      return dynamic_cast<VariantBoolean*>(lookup(name));
    }




    VariantDate* VariantArray::lookupDate (string const& name) {
      return dynamic_cast<VariantDate*>(lookup(name));
    }




    VariantDatetime* VariantArray::lookupDatetime (string const& name) {
      return dynamic_cast<VariantDatetime*>(lookup(name));
    }




    VariantDouble* VariantArray::lookupDouble (string const& name) {
      return dynamic_cast<VariantDouble*>(lookup(name));
    }




    VariantFloat* VariantArray::lookupFloat (string const& name) {
      return dynamic_cast<VariantFloat*>(lookup(name));
    }




    VariantInt16* VariantArray::lookupInt16 (string const& name) {
      return dynamic_cast<VariantInt16*>(lookup(name));
    }


    VariantInt8* VariantArray::lookupInt8 (string const& name) {
      return dynamic_cast<VariantInt8*>(lookup(name));
    }


    VariantInt32* VariantArray::lookupInt32 (string const& name) {
      return dynamic_cast<VariantInt32*>(lookup(name));
    }




    VariantInt64* VariantArray::lookupInt64 (string const& name) {
      return dynamic_cast<VariantInt64*>(lookup(name));
    }




    VariantMatrix2* VariantArray::lookupMatrix2 (string const& name) {
      return dynamic_cast<VariantMatrix2*>(lookup(name));
    }




    VariantNull* VariantArray::lookupNull (string const& name) {
      return dynamic_cast<VariantNull*>(lookup(name));
    }




    VariantString* VariantArray::lookupString (string const& name) {
      return dynamic_cast<VariantString*>(lookup(name));
    }



    string const& VariantArray::lookupString (string const& name, bool& found) {
      static string const empty = "";

      VariantObject* o = lookup(name);

      if (o == 0) {
        found = false;
        return empty;
      }
      else {
        if (o->is<VariantString>()) {
          found = true;
          return dynamic_cast<VariantString*>(o)->getValue();
        }
        else {
          found = false;
          return empty;
        }
      }
    }



    VariantUInt8* VariantArray::lookupUInt8 (string const& name) {
      return dynamic_cast<VariantUInt8*>(lookup(name));
    }



    VariantUInt16* VariantArray::lookupUInt16 (string const& name) {
      return dynamic_cast<VariantUInt16*>(lookup(name));
    }




    VariantUInt32* VariantArray::lookupUInt32 (string const& name) {
      return dynamic_cast<VariantUInt32*>(lookup(name));
    }




    VariantUInt64* VariantArray::lookupUInt64 (string const& name) {
      return dynamic_cast<VariantUInt64*>(lookup(name));
    }




    VariantVector* VariantArray::lookupVector (string const& name) {
      return dynamic_cast<VariantVector*>(lookup(name));
    }



    vector<string> VariantArray::lookupStrings (string const& name, bool& error) {
      vector<string> result;
      error = false;

      VariantVector* attr = lookupVector(name);

      if (attr == 0) {
        error = true;
        return result;
      }

      vector<VariantObject*> const& elements = attr->getValues();

      for (vector<VariantObject*>::const_iterator i = elements.begin();  i != elements.end();  ++i) {
        VariantObject* element = *i;
        VariantString* s = dynamic_cast<VariantString*>(element);

        if (s == 0) {
          error = true;
        }
        else {
          result.push_back(s->getValue());
        }
      }

      return result;
    }
  }
}

