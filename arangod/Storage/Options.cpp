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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Options.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Storage/Marker.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/vocbase.h"

#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
  
static std::unique_ptr<VPackOptions> insertOptions;

// custom type value handler, used for deciphering the _id attribute
struct CustomTypeHandler : public VPackCustomTypeHandler {
  explicit CustomTypeHandler(TRI_vocbase_t* vocbase)
      : vocbase(vocbase), resolver(nullptr), ownsResolver(false) {}
  
  CustomTypeHandler(TRI_vocbase_t* vocbase, CollectionNameResolver* resolver)
      : vocbase(vocbase), resolver(resolver), ownsResolver(false) {}

  ~CustomTypeHandler() { 
    if (ownsResolver) {
      delete resolver; 
    }
  }

  CollectionNameResolver* getResolver() {
    if (resolver == nullptr) {
      resolver = new CollectionNameResolver(vocbase);
      ownsResolver = true;
    }
    return resolver;
  }

  void dump(VPackSlice const& value, VPackDumper* dumper,
            VPackSlice const& base) override final {
    if (value.head() != 0xf3) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid custom type");
    }

    // _id
    if (!base.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid value type");
    }
  
    uint64_t cid = MarkerHelper::readNumber<uint64_t>(value.begin() + 1, sizeof(uint64_t));
    char buffer[512];  // This is enough for collection name + _key
    size_t len = getResolver()->getCollectionName(&buffer[0], cid);
    buffer[len] = '/';
    VPackSlice key = base.get(TRI_VOC_ATTRIBUTE_KEY);

    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);
    if (p == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid _key value");
    }
    memcpy(&buffer[len + 1], p, keyLength);
    dumper->appendString(&buffer[0], len + 1 + keyLength);
  }
  
  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    if (value.head() != 0xf3) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid custom type");
    }

    // _id
    if (!base.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid value type");
    }
    
    uint64_t cid = MarkerHelper::readNumber<uint64_t>(value.begin() + 1, sizeof(uint64_t));
    std::string result(getResolver()->getCollectionName(cid));
    result.push_back('/');
    VPackSlice key = base.get(TRI_VOC_ATTRIBUTE_KEY);

    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);
    if (p == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid _key value");
    }
    result.append(p, keyLength);
    return result;
  }

  TRI_vocbase_t* vocbase;
  CollectionNameResolver* resolver;
  bool ownsResolver;
};


// initialize global vpack options
void StorageOptions::initialize() {
  // initialize options for inserting documents
  insertOptions.reset(new VPackOptions);
  insertOptions->buildUnindexedArrays = false;
  insertOptions->buildUnindexedObjects = false;
  insertOptions->checkAttributeUniqueness = true;
  insertOptions->sortAttributeNames = true; 
  insertOptions->customTypeHandler = nullptr;
  insertOptions->attributeExcludeHandler = basics::VelocyPackHelper::getExcludeHandler();
}

VPackCustomTypeHandler* StorageOptions::getCustomTypeHandler(TRI_vocbase_t* vocbase) {
  return new CustomTypeHandler(vocbase);
}

VPackOptions const* StorageOptions::getInsertOptions() {
  return insertOptions.get();
}

/*
  // set options for JSON to document conversion
  JsonToDocumentTemplate.buildUnindexedArrays = false;
  JsonToDocumentTemplate.buildUnindexedObjects = false;
  JsonToDocumentTemplate.checkAttributeUniqueness = true;
  JsonToDocumentTemplate.sortAttributeNames = true;
  JsonToDocumentTemplate.attributeTranslator = _translator.get();
  JsonToDocumentTemplate.customTypeHandler = nullptr;
  JsonToDocumentTemplate.attributeExcludeHandler = _excludeHandler.get();

  // set options for document to JSON conversion
  DocumentToJsonTemplate.attributeTranslator = _translator.get();
  DocumentToJsonTemplate.customTypeHandler = nullptr;
  DocumentToJsonTemplate.attributeExcludeHandler = nullptr;
  DocumentToJsonTemplate.prettyPrint = false;
  DocumentToJsonTemplate.escapeForwardSlashes = true;
  DocumentToJsonTemplate.unsupportedTypeBehavior =
      VPackOptions::FailOnUnsupportedType;

  // non-document options
  NonDocumentTemplate.buildUnindexedArrays = true;
  NonDocumentTemplate.buildUnindexedObjects = true;
  NonDocumentTemplate.checkAttributeUniqueness = false;
  NonDocumentTemplate.sortAttributeNames = false;
  NonDocumentTemplate.attributeTranslator = nullptr;
  NonDocumentTemplate.customTypeHandler = nullptr;
  NonDocumentTemplate.attributeExcludeHandler = nullptr;
  NonDocumentTemplate.prettyPrint = false;
  NonDocumentTemplate.escapeForwardSlashes = true;
  NonDocumentTemplate.unsupportedTypeBehavior =
      VPackOptions::FailOnUnsupportedType;
*/
