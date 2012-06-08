////////////////////////////////////////////////////////////////////////////////
/// @brief input parsers
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "InputParser.h"

#include <Basics/Exceptions.h>
#include <Variant/VariantArray.h>
#include <Variant/VariantBoolean.h>
#include <Variant/VariantDouble.h>
#include <Variant/VariantInt64.h>
#include <Variant/VariantNull.h>
#include <Variant/VariantObject.h>
#include <Variant/VariantString.h>
#include <Variant/VariantVector.h>
#include <Rest/HttpRequest.h>

#include "JsonParserX/JsonParserXDriver.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// helper functions
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    enum ObjectDescriptionType {
      OD_BOOLEAN,
      OD_DOUBLE,
      OD_INTEGER,
      OD_STRING,
      OD_STRING_LIST,
      OD_VARIANT_ARRAY,
      OD_VARIANT_BOOLEAN,
      OD_VARIANT_DOUBLE,
      OD_VARIANT_INTEGER,
      OD_VARIANT_NULL,
      OD_VARIANT_STRING,
      OD_VARIANT_STRING_LIST,
      OD_VARIANT_VECTOR
    };



    struct AttributeDescription {
      AttributeDescription () {
      }

      AttributeDescription (ObjectDescriptionType type, void* store, bool* hasAttribute)
        : type(type), store(store), hasAttribute(hasAttribute) {
      }

      ObjectDescriptionType type;
      void* store;
      bool* hasAttribute;
    };
  }
}



namespace {
  void clearObject (ObjectDescriptionType type, void* store) {
    switch (type) {
      case OD_BOOLEAN:
        *reinterpret_cast<bool*>(store) = false;
        break;

      case OD_DOUBLE:
        *reinterpret_cast<double*>(store) = false;
        break;

      case OD_INTEGER:
        *reinterpret_cast<int64_t*>(store) = 0;
        break;

      case OD_STRING:
        *reinterpret_cast<string*>(store) = "";
        break;

      case OD_VARIANT_ARRAY:
      case OD_VARIANT_BOOLEAN:
      case OD_VARIANT_DOUBLE:
      case OD_VARIANT_INTEGER:
      case OD_VARIANT_NULL:
      case OD_VARIANT_STRING:
      case OD_VARIANT_VECTOR:
        *reinterpret_cast<void**>(store) = 0;
        break;

      case OD_STRING_LIST:
      case OD_VARIANT_STRING_LIST:
        break;
    }
  }



  bool checkObjectType (string const& name, ObjectDescriptionType type, VariantObject* object, string& errorMessage) {
    char const* expecting;
    VariantObject::ObjectType otype;

    switch (type) {
      case OD_VARIANT_ARRAY:
        expecting = "ARRAY";
        otype = VariantObject::VARIANT_ARRAY;
        break;

      case OD_BOOLEAN:
      case OD_VARIANT_BOOLEAN:
        expecting = "BOOLEAN";
        otype = VariantObject::VARIANT_BOOLEAN;
        break;

      case OD_DOUBLE:
      case OD_VARIANT_DOUBLE:
        expecting = "DOUBLE";
        otype = VariantObject::VARIANT_DOUBLE;
        break;

      case OD_INTEGER:
      case OD_VARIANT_INTEGER:
        expecting = "INTEGER";
        otype = VariantObject::VARIANT_INT64;
        break;

      case OD_VARIANT_NULL:
        expecting = "NULL";
        otype = VariantObject::VARIANT_NULL;
        break;

      case OD_STRING:
      case OD_VARIANT_STRING:
        expecting = "STRING";
        otype = VariantObject::VARIANT_STRING;
        break;

      case OD_STRING_LIST:
      case OD_VARIANT_STRING_LIST:
        expecting = "VECTOR OF STRINGS";
        otype = VariantObject::VARIANT_VECTOR;
        break;

      case OD_VARIANT_VECTOR:
        expecting = "VECTOR";
        otype = VariantObject::VARIANT_VECTOR;
        break;

      default:
        THROW_INTERNAL_ERROR("wrong type");
    }

    if (otype != object->type()) {
      errorMessage = "attribute '"
      + name
      + "' is of wrong type (expecting "
      + expecting
      + ", got "
      + VariantObject::NameObjectType(object->type())
      + ")";

      return false;
    }
    else {
      return true;
    }
  }



  template<typename VT>
  bool extractObjects (string const& name,
                       VariantVector* list,
                       ObjectDescriptionType type,
                       vector< VT* >* store,
                       string& errorMessage) {
    vector<VariantObject*> const& values = list->getValues();

    for (vector<VariantObject*>::const_iterator i = values.begin();  i != values.end();  ++i) {
      VariantObject* object = *i;

      bool ok = checkObjectType(name, type, object, errorMessage);

      if (! ok) {
        return false;
      }

      VT* vt = object->as<VT>();

      store->push_back(vt);
    }

    return true;
  }



  template<typename VT, typename T>
  bool extractObjects (string const& name,
                       VariantVector* list,
                       ObjectDescriptionType type,
                       vector< T >* store,
                       string& errorMessage) {
    vector<VariantObject*> const& values = list->getValues();

    for (vector<VariantObject*>::const_iterator i = values.begin();  i != values.end();  ++i) {
      VariantObject* object = *i;

      bool ok = checkObjectType(name, type, object, errorMessage);

      if (! ok) {
        return false;
      }

      VT* vt = object->as<VT>();

      store->push_back(vt->getValue());
    }

    return true;
  }



  bool extractObject (string const& name,
                      VariantObject* object,
                      AttributeDescription const& desc,
                      string& errorMessage) {

    switch (desc.type) {
      case OD_VARIANT_ARRAY:
        *reinterpret_cast<VariantArray**>(desc.store) = object->as<VariantArray>();
        return true;

      case OD_BOOLEAN:
        *reinterpret_cast<bool*>(desc.store) = object->as<VariantBoolean>()->getValue();
        return true;

      case OD_VARIANT_BOOLEAN:
        *reinterpret_cast<VariantBoolean**>(desc.store) = object->as<VariantBoolean>();
        return true;

      case OD_DOUBLE:
        *reinterpret_cast<double*>(desc.store) = object->as<VariantDouble>()->getValue();
        return true;

      case OD_VARIANT_DOUBLE:
        *reinterpret_cast<VariantDouble**>(desc.store) = object->as<VariantDouble>();
        return true;

      case OD_INTEGER:
        *reinterpret_cast<int64_t*>(desc.store) = object->as<VariantInt64>()->getValue();
        return true;

      case OD_VARIANT_INTEGER:
        *reinterpret_cast<VariantInt64**>(desc.store) = object->as<VariantInt64>();
        return true;

      case OD_VARIANT_NULL:
        *reinterpret_cast<VariantNull**>(desc.store) = object->as<VariantNull>();
        return true;

      case OD_STRING:
        *reinterpret_cast<string*>(desc.store) = object->as<VariantString>()->getValue();
        return true;

      case OD_VARIANT_STRING:
        *reinterpret_cast<VariantString**>(desc.store) = object->as<VariantString>();
        return true;

      case OD_STRING_LIST:
        return extractObjects<VariantString>(name,
                                             object->as<VariantVector>(),
                                             OD_VARIANT_STRING,
                                             reinterpret_cast< vector<string>* >(desc.store),
                                             errorMessage);

      case OD_VARIANT_STRING_LIST:
        return extractObjects(name,
                              object->as<VariantVector>(),
                              OD_VARIANT_STRING,
                              reinterpret_cast< vector<VariantString*>* >(desc.store),
                              errorMessage);

      case OD_VARIANT_VECTOR:
        *reinterpret_cast<VariantVector**>(desc.store) = object->as<VariantVector>();
        return true;

      default:
        THROW_INTERNAL_ERROR("wrong type");
    }
  }



  bool loadObject (VariantArray* array, string const& name, AttributeDescription const& desc, bool optional, string& errorMessage) {
    clearObject(desc.type, desc.store);

    if (desc.hasAttribute != 0) {
      *desc.hasAttribute = false;
    }

    VariantObject* object = array->lookup(name);

    if (object == 0) {
      if (optional) {
        return true;
      }
      else {
        errorMessage = "attribute '" + name + "' not found";

        return false;
      }
    }

    if (object->is<VariantNull>() && optional) {
      return true;
    }

    if (desc.hasAttribute != 0) {
      *desc.hasAttribute = true;
    }

    bool ok = checkObjectType(name, desc.type, object, errorMessage);

    if (! ok) {
      return false;
    }

    return extractObject(name, object, desc, errorMessage);
  }



  bool loadAlternatives (VariantArray* array,
                         string const& name,
                         vector<AttributeDescription> const& alternatives,
                         string& errorMessage) {
    for (vector<AttributeDescription>::const_iterator i = alternatives.begin();  i != alternatives.end();  ++i) {
      clearObject(i->type, i->store);
    }

    VariantObject* object = array->lookup(name);

    if (object == 0) {
      return true;
    }

    for (vector<AttributeDescription>::const_iterator i = alternatives.begin();  i != alternatives.end();  ++i) {
      AttributeDescription const& desc = *i;

      if (checkObjectType(name, desc.type, object, errorMessage)) {
        return extractObject(name, object, desc, errorMessage);
      }
    }

    errorMessage = "attribute '" + name + "' is of wrong type";

    return false;
  }
}

namespace triagens {
  namespace rest {
    namespace InputParser {

      // -----------------------------------------------------------------------------
      // object description implementation
      // -----------------------------------------------------------------------------

      struct ObjectDescriptionImpl {
        map< string, AttributeDescription > attributes;
        map< string, AttributeDescription > optionals;
        map< string, vector<AttributeDescription> > alternatives;

        string lastError;
      };

      // -----------------------------------------------------------------------------
      // object description
      // -----------------------------------------------------------------------------

      ObjectDescription::ObjectDescription () {
        impl = new ObjectDescriptionImpl();
      }



      ObjectDescription::~ObjectDescription () {
        delete impl;
      }



      string const& ObjectDescription::lastError () {
        return impl->lastError;
      }



      bool ObjectDescription::parse (VariantObject* object) {
        impl->lastError.clear();

        // object must be a json array
        if (object == 0) {
          impl->lastError = "cannot parse object";

          return false;
        }

        if (! object->is<VariantArray>()) {
          impl->lastError = "not an object";

          return false;
        }

        VariantArray* array = object->as<VariantArray>();

        // now find the attributes, optionals, and alternatives
        for (map<string, AttributeDescription>::iterator i = impl->attributes.begin();  i != impl->attributes.end();  ++i) {
          string const& name = i->first;
          AttributeDescription& desc = i->second;

          bool ok = loadObject(array, name, desc, false, impl->lastError);

          if (! ok) {
            return false;
          }
        }

        for (map<string, AttributeDescription>::iterator i = impl->optionals.begin();  i != impl->optionals.end();  ++i) {
          string const& name = i->first;
          AttributeDescription& desc = i->second;

          bool ok = loadObject(array, name, desc, true, impl->lastError);

          if (! ok) {
            return false;
          }
        }

        for (map< string, vector<AttributeDescription> >::iterator i = impl->alternatives.begin();  i != impl->alternatives.end();  ++i) {
          string const& name = i->first;

          bool ok = loadAlternatives(array, name, i->second, impl->lastError);

          if (! ok) {
            return false;
          }
        }

        transform();

        return true;
      }



      void ObjectDescription::transform () {
      }

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // attribute
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

      ObjectDescription& ObjectDescription::attribute (string const& name, VariantArray*& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_ARRAY, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, VariantBoolean*& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_BOOLEAN, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, bool& store) {
        impl->attributes[name] = AttributeDescription(OD_BOOLEAN, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, VariantDouble*& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_DOUBLE, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, double& store) {
        impl->attributes[name] = AttributeDescription(OD_DOUBLE, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, VariantInt64*& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_INTEGER, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, int64_t& store) {
        impl->attributes[name] = AttributeDescription(OD_INTEGER, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, VariantNull*& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_NULL, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, VariantString*& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_STRING, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, string& store) {
        impl->attributes[name] = AttributeDescription(OD_STRING, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, vector<VariantString*>& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_STRING_LIST, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, vector<string>& store) {
        impl->attributes[name] = AttributeDescription(OD_STRING_LIST, reinterpret_cast<void*>(&store), 0);
        return *this;
      }



      ObjectDescription& ObjectDescription::attribute (string const& name, VariantVector*& store) {
        impl->attributes[name] = AttributeDescription(OD_VARIANT_VECTOR, reinterpret_cast<void*>(&store), 0);
        return *this;
      }

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // optional
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


      ObjectDescription& ObjectDescription::optional (string const& name, VariantArray*& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_ARRAY, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, VariantBoolean*& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_BOOLEAN, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, bool& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_BOOLEAN, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, VariantDouble*& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_DOUBLE, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, double& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_DOUBLE, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, VariantInt64*& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_INTEGER, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, int64_t& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_INTEGER, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, VariantNull*& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_NULL, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, VariantString*& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_STRING, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, string& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_STRING, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, vector<VariantString*>& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_STRING_LIST, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, vector<string>& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_STRING_LIST, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }



      ObjectDescription& ObjectDescription::optional (string const& name, VariantVector*& store, bool* hasAttribute) {
        impl->optionals[name] = AttributeDescription(OD_VARIANT_VECTOR, reinterpret_cast<void*>(&store), hasAttribute);
        return *this;
      }

      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // alternative
      // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


      ObjectDescription& ObjectDescription::alternative (string const& name, VariantArray*& store) {
        impl->alternatives[name].push_back(AttributeDescription(OD_VARIANT_ARRAY, reinterpret_cast<void*>(&store), 0));
        return *this;
      }



      ObjectDescription& ObjectDescription::alternative (string const& name, VariantBoolean*& store) {
        impl->alternatives[name].push_back(AttributeDescription(OD_VARIANT_BOOLEAN, reinterpret_cast<void*>(&store), 0));
        return *this;
      }



      ObjectDescription& ObjectDescription::alternative (string const& name, VariantInt64*& store) {
        impl->alternatives[name].push_back(AttributeDescription(OD_VARIANT_INTEGER, reinterpret_cast<void*>(&store), 0));
        return *this;
      }



      ObjectDescription& ObjectDescription::alternative (string const& name, VariantNull*& store) {
        impl->alternatives[name].push_back(AttributeDescription(OD_VARIANT_NULL, reinterpret_cast<void*>(&store), 0));
        return *this;
      }



      ObjectDescription& ObjectDescription::alternative (string const& name, VariantString*& store) {
        impl->alternatives[name].push_back(AttributeDescription(OD_VARIANT_STRING, reinterpret_cast<void*>(&store), 0));
        return *this;
      }



      ObjectDescription& ObjectDescription::alternative (string const& name, VariantVector*& store) {
        impl->alternatives[name].push_back(AttributeDescription(OD_VARIANT_VECTOR, reinterpret_cast<void*>(&store), 0));
        return *this;
      }

      // -----------------------------------------------------------------------------
      // public fucntions
      // -----------------------------------------------------------------------------

      VariantObject* json (string const& input) {
        JsonParserXDriver parser;

        return parser.parse(input);
      }



      VariantObject* json (HttpRequest* request) {
        JsonParserXDriver parser;

        return parser.parse(request->body());
      }



      VariantArray* jsonArray (string const& input) {
        JsonParserXDriver parser;

        VariantObject* object = parser.parse(input);

        if (object == 0) {
          return 0;
        }
        else if (object->type() == VariantObject::VARIANT_ARRAY) {
          return dynamic_cast<VariantArray*>(object);
        }
        else {
          delete object;
          return 0;
        }
      }



      VariantArray* jsonArray (HttpRequest* request) {
        JsonParserXDriver parser;

        VariantObject* object = parser.parse(request->body());

        if (object == 0) {
          return 0;
        }
        else if (object->type() == VariantObject::VARIANT_ARRAY) {
          return dynamic_cast<VariantArray*>(object);
        }
        else {
          delete object;
          return 0;
        }
      }
    }
  }
}
