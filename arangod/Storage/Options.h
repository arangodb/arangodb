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

#ifndef ARANGOD_STORAGE_OPTIONS_H
#define ARANGOD_STORAGE_OPTIONS_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace velocypack {
class AttributeTranslator;
struct AttributeExcludeHandler;
struct CustomTypeHandler;
struct Options;
}
class CollectionNameResolver;

class StorageOptions {
 public:
  StorageOptions();
  ~StorageOptions();

  StorageOptions(StorageOptions const&) = delete;
  StorageOptions& operator=(StorageOptions const&) = delete;

  static arangodb::velocypack::Options getDocumentToJsonTemplate();
  static arangodb::velocypack::Options getJsonToDocumentTemplate();
  static arangodb::velocypack::Options getNonDocumentTemplate();
  static arangodb::velocypack::CustomTypeHandler* createCustomHandler(
      arangodb::CollectionNameResolver const*);

 private:
  std::unique_ptr<arangodb::velocypack::AttributeTranslator> _translator;
  std::unique_ptr<arangodb::velocypack::AttributeExcludeHandler> _excludeHandler;

  static arangodb::velocypack::Options JsonToDocumentTemplate;
  static arangodb::velocypack::Options DocumentToJsonTemplate;
  static arangodb::velocypack::Options NonDocumentTemplate;
};
}

#endif


