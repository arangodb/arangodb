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

#include <iostream>
#include "Storage/Options.h"
#include "Basics/Exceptions.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/voc-types.h"

#include <velocypack/Dumper.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

// global options used when converting JSON into a document
VPackOptions StorageOptions::JsonToDocumentTemplate;
// global options used when converting documents into JSON
VPackOptions StorageOptions::DocumentToJsonTemplate;
// global options used for other conversions
VPackOptions StorageOptions::NonDocumentTemplate;
  
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
  CustomTypeHandlerImpl (triagens::arango::CollectionNameResolver const* resolver) 
    : resolver(resolver) { 
  }

  void toJson (VPackSlice const& value, VPackDumper* dumper, VPackSlice const& base) {
    std::cout << "TOJSON. VALUE.HEAD(): " << (int) value.head() << "\n";
    if (value.head() == 0xf0) {
      std::cout << "0xf0\n";
      // _id
      if (! base.isObject()) {
      std::cout << "NO OBJECT. " << base << "\n";
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid value type"); 
      }
      uint64_t cid = arangodb::velocypack::readUInt64(value.start() + 1);
      std::cout << "HAVE READ CID: " << cid << "\n";
      char buffer[512]; // TODO: check if size is appropriate
      size_t len = resolver->getCollectionName(&buffer[0], cid); 
      buffer[len] = '/';
      VPackSlice key = base.get(TRI_VOC_ATTRIBUTE_KEY);
      
      VPackValueLength keyLength;
      char const* p = key.getString(keyLength);
      if (p == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid _key value"); 
      }
      memcpy(&buffer[len + 1], p, keyLength);
      dumper->appendString(&buffer[0], len + 1 + keyLength);
      return;
    }

    if (value.head() == 0xf1) {
      std::cout << "0xf1\n";
      // _rev
      dumper->sink()->push_back('"');
      dumper->appendUInt(arangodb::velocypack::readUInt64(value.start() + 1));
      dumper->sink()->push_back('"');
      return;
    }

    if (value.head() == 0xf2) {
      std::cout << "0xf2\n";
      // _from, _to
      // TODO
      return;
    }
      std::cout << "OOPS\n";
    throw "unknown type!";
  }

  VPackValueLength byteSize (VPackSlice const& value) {
    if (value.head() == 0xf0) {
      // _id
      return 1 + 8;  // 0xf0 + 8 bytes for collection id
    }
    
    if (value.head() == 0xf1) {
      // _rev
      return 1 + 8;  // 0xf1 + 8 bytes for tick value
    }

    if (value.head() == 0xf2) {
      // _from, _to
      // TODO!!
      return 1;
    }

    throw "unknown type!";
  }

  triagens::arango::CollectionNameResolver const* resolver;
  TRI_voc_cid_t cid;
};

StorageOptions::StorageOptions () 
  : _translator(new VPackAttributeTranslator),
    _excludeHandler(new ExcludeHandlerImpl) {

  // these attribute names will be translated into short integer values
  _translator->add(TRI_VOC_ATTRIBUTE_KEY,  1);
  _translator->add(TRI_VOC_ATTRIBUTE_REV,  2);
  _translator->add(TRI_VOC_ATTRIBUTE_ID,   3);
  _translator->add(TRI_VOC_ATTRIBUTE_FROM, 4);
  _translator->add(TRI_VOC_ATTRIBUTE_TO,   5);

  _translator->seal();

  // set options for JSON to document conversion
  JsonToDocumentTemplate.buildUnindexedArrays     = false;
  JsonToDocumentTemplate.buildUnindexedObjects    = false;
  JsonToDocumentTemplate.checkAttributeUniqueness = true;
  JsonToDocumentTemplate.sortAttributeNames       = true;
  JsonToDocumentTemplate.attributeTranslator      = _translator.get();
  JsonToDocumentTemplate.customTypeHandler        = nullptr;
  JsonToDocumentTemplate.attributeExcludeHandler  = _excludeHandler.get();

  // set options for document to JSON conversion
  DocumentToJsonTemplate.attributeTranslator      = _translator.get();
  DocumentToJsonTemplate.customTypeHandler        = nullptr;
  DocumentToJsonTemplate.attributeExcludeHandler  = nullptr;
  DocumentToJsonTemplate.prettyPrint              = false;
  DocumentToJsonTemplate.escapeForwardSlashes     = true;
  DocumentToJsonTemplate.unsupportedTypeBehavior  = VPackOptions::FailOnUnsupportedType;

  // non-document options
  NonDocumentTemplate.buildUnindexedArrays        = true;
  NonDocumentTemplate.buildUnindexedObjects       = true;
  NonDocumentTemplate.checkAttributeUniqueness    = false;
  NonDocumentTemplate.sortAttributeNames          = false;
  NonDocumentTemplate.attributeTranslator         = nullptr;
  NonDocumentTemplate.customTypeHandler           = nullptr;
  NonDocumentTemplate.attributeExcludeHandler     = nullptr;
  NonDocumentTemplate.prettyPrint                 = false;
  NonDocumentTemplate.escapeForwardSlashes        = true;
  NonDocumentTemplate.unsupportedTypeBehavior     = VPackOptions::FailOnUnsupportedType;
}

StorageOptions::~StorageOptions () {
}
      
VPackOptions StorageOptions::getDocumentToJsonTemplate () {
  return DocumentToJsonTemplate;
}

VPackOptions StorageOptions::getJsonToDocumentTemplate () {
  return JsonToDocumentTemplate;
}

VPackOptions StorageOptions::getNonDocumentTemplate () {
  return NonDocumentTemplate;
}

VPackCustomTypeHandler* StorageOptions::createCustomHandler (triagens::arango::CollectionNameResolver const* resolver) {
  return new CustomTypeHandlerImpl(resolver);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
