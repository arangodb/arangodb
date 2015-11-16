////////////////////////////////////////////////////////////////////////////////
/// @brief Options for VPack storage
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Storage/Options.h"

using namespace arangodb;

// global options used when converting JSON into a document
VPackOptions JsonToDocumentOptions;
// global options used when converting documents into JSON
VPackOptions DocumentToJsonOptions;
// global options used for other conversions
VPackOptions NonDocumentOptions;
  
struct ExcludeHandlerImpl : public VPackAttributeExcludeHandler {
  bool shouldExclude (VPackSlice const& key, int nesting) override final {
    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);

    if (p == nullptr || *p != '_' || keyLength < 3 || keyLength > 5) {
      // keep attribute
      return true;
    } 
  
    if ((keyLength == 3 && memcmp(p, "_id",   keyLength) == 0) ||
        (keyLength == 4 && memcmp(p, "_rev",  keyLength) == 0) ||
        (keyLength == 3 && memcmp(p, "_to",   keyLength) == 0) ||
        (keyLength == 5 && memcmp(p, "_from", keyLength) == 0)) {
      // exclude these attribute
      return true;
    }

    // keep attribute
    return false;
  }
};

struct CustomTypeHandlerImpl : public VPackCustomTypeHandler {
  void toJson (VPackSlice const& value, VPackDumper* dumper, VPackSlice const& base) {
    if (value.head() == 0xf0) {
      // _id
      // TODO
      return;
    }

    if (value.head() == 0xf1) {
      // _rev
      // TODO
      return;
    }

    if (value.head() == 0xf2) {
      // _from, _to
      // TODO
      return;
    }
    throw "unknown type!";
  }

  VPackValueLength byteSize (VPackSlice const& value) {
    if (value.head() == 0xf0) {
      // _id. this type uses 1 byte only
      return 1; 
    }
    
    if (value.head() == 0xf1) {
      // _rev
      return 8;
    }

    if (value.head() == 0xf2) {
      // _from, _to
      // TODO!!
      return 1;
    }
    throw "unknown type!";
  }
};
      

StorageOptions::StorageOptions () 
  : _translator(new VPackAttributeTranslator),
    _customTypeHandler(new CustomTypeHandlerImpl),
    _excludeHandler(new ExcludeHandlerImpl) {

  // these attribute names will be translated into short integer values
  _translator->add("_key",  1);
  _translator->add("_rev",  2);
  _translator->add("_id",   3);
  _translator->add("_from", 4);
  _translator->add("_to",   5);
  _translator->seal();

  // set options for JSON to document conversion
  JsonToDocumentOptions.buildUnindexedArrays     = false;
  JsonToDocumentOptions.buildUnindexedObjects    = false;
  JsonToDocumentOptions.checkAttributeUniqueness = true;
  JsonToDocumentOptions.sortAttributeNames       = true;
  JsonToDocumentOptions.attributeTranslator      = _translator.get();
  JsonToDocumentOptions.customTypeHandler        = _customTypeHandler.get();
  JsonToDocumentOptions.attributeExcludeHandler  = _excludeHandler.get();

  // set options for document to JSON conversion
  DocumentToJsonOptions.prettyPrint              = false;
  DocumentToJsonOptions.escapeForwardSlashes     = true;
  DocumentToJsonOptions.unsupportedTypeBehavior  = VPackOptions::FailOnUnsupportedType;
  DocumentToJsonOptions.attributeTranslator      = _translator.get();
  DocumentToJsonOptions.customTypeHandler        = _customTypeHandler.get();
  DocumentToJsonOptions.attributeExcludeHandler  = nullptr;


  NonDocumentOptions.prettyPrint                 = false;
  NonDocumentOptions.escapeForwardSlashes        = true;
  NonDocumentOptions.sortAttributeNames          = false;
  NonDocumentOptions.buildUnindexedArrays        = true;
  NonDocumentOptions.buildUnindexedObjects       = true;
  NonDocumentOptions.checkAttributeUniqueness    = false;
  NonDocumentOptions.unsupportedTypeBehavior     = VPackOptions::FailOnUnsupportedType;
  NonDocumentOptions.attributeTranslator         = nullptr;
  NonDocumentOptions.customTypeHandler           = nullptr;
  NonDocumentOptions.attributeExcludeHandler     = nullptr;
//      bool keepTopLevelOpen         = false;
}

StorageOptions::~StorageOptions () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
