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
#include "Utils/CollectionNameResolver.h"
#include "VocBase/vocbase.h"

#include <velocypack/AttributeTranslator.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
  
static std::unique_ptr<VPackAttributeTranslator> translator;
static std::unique_ptr<VPackAttributeExcludeHandler> excludeHandler;
static std::unique_ptr<VPackCustomTypeHandler> customLengthHandler;
static std::unique_ptr<VPackOptions> defaultOptions;
static std::unique_ptr<VPackOptions> insertOptions;

// attribute exclude handler for skipping over system attributes
struct SystemAttributeExcludeHandler : public VPackAttributeExcludeHandler {
  bool shouldExclude(VPackSlice const& key, int nesting) override final {
    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);

    if (p == nullptr || *p != '_' || keyLength < 3 || keyLength > 5 || nesting > 0) {
      // keep attribute
      return true;
    }

    // exclude these attributes (but not _key!)
    if ((keyLength == 3 && memcmp(p, TRI_VOC_ATTRIBUTE_ID, keyLength) == 0) ||
        (keyLength == 4 && memcmp(p, TRI_VOC_ATTRIBUTE_REV, keyLength) == 0) ||
        (keyLength == 3 && memcmp(p, TRI_VOC_ATTRIBUTE_TO, keyLength) == 0) ||
        (keyLength == 5 && memcmp(p, TRI_VOC_ATTRIBUTE_FROM, keyLength) == 0)) {
      return true;
    }

    // keep attribute
    return false;
  }
};


// custom type value handler, used for determining the length of the _id attribute
struct CustomIdLengthHandler : public VPackCustomTypeHandler {
  void toJson(VPackSlice const&, VPackDumper*,
              VPackSlice const&) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  VPackValueLength byteSize(VPackSlice const& value) {
    if (value.head() != 0xf0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid custom type");
    }
    
    // _id
    return 1 + 8;  // 0xf0 + 8 bytes for collection id
  }
};

// custom type value handler, used for deciphering the _id attribute
struct CustomIdTypeHandler : public VPackCustomTypeHandler {
  explicit CustomIdTypeHandler(TRI_vocbase_t* vocbase)
      : vocbase(vocbase), resolver(nullptr), ownsResolver(false) {}
  
  CustomIdTypeHandler(TRI_vocbase_t* vocbase, CollectionNameResolver* resolver)
      : vocbase(vocbase), resolver(resolver), ownsResolver(false) {}

  ~CustomIdTypeHandler() { 
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

  void toJson(VPackSlice const& value, VPackDumper* dumper,
              VPackSlice const& base) {
    if (value.head() != 0xf0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid custom type");
    }

    // _id
    if (!base.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid value type");
    }
    uint64_t cid = arangodb::velocypack::readUInt64(value.start() + 1);
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

  VPackValueLength byteSize(VPackSlice const& value) {
    if (value.head() != 0xf0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid custom type");
    }
    
    // _id
    return 1 + 8;  // 0xf0 + 8 bytes for collection id
  }

  TRI_vocbase_t* vocbase;
  CollectionNameResolver* resolver;
  bool ownsResolver;
};


// initialize global vpack options
void StorageOptions::initialize() {
  // initialize translator
  translator.reset(new VPackAttributeTranslator);

  // these attribute names will be translated into short integer values
  translator->add(TRI_VOC_ATTRIBUTE_KEY, 1);
  translator->add(TRI_VOC_ATTRIBUTE_REV, 2);
  translator->add(TRI_VOC_ATTRIBUTE_ID, 3);
  translator->add(TRI_VOC_ATTRIBUTE_FROM, 4);
  translator->add(TRI_VOC_ATTRIBUTE_TO, 5);

  translator->seal();
  
  // initialize system attribute exclude handler    
  excludeHandler.reset(new SystemAttributeExcludeHandler);

  customLengthHandler.reset(new CustomIdLengthHandler);

  // initialize default options
  defaultOptions.reset(new VPackOptions);
  defaultOptions->attributeTranslator = nullptr;
  defaultOptions->customTypeHandler = nullptr;
  
  // initialize options for inserting documents
  insertOptions.reset(new VPackOptions);
  insertOptions->buildUnindexedArrays = false;
  insertOptions->buildUnindexedObjects = false;
  insertOptions->checkAttributeUniqueness = true;
  insertOptions->sortAttributeNames = true; 
  insertOptions->attributeTranslator = translator.get();
  insertOptions->customTypeHandler = customLengthHandler.get();
  insertOptions->attributeExcludeHandler = excludeHandler.get();
}

VPackAttributeTranslator const* StorageOptions::getTranslator() {
  return translator.get();
}

VPackOptions const* StorageOptions::getDefaultOptions() {
  return defaultOptions.get();
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
