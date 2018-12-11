////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_OPTIONS_H
#define VELOCYPACK_OPTIONS_H 1

#include <string>
#include <cstdint>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"

namespace arangodb {
namespace velocypack {
class AttributeTranslator;
class Dumper;
struct Options;
class Slice;

struct AttributeExcludeHandler {
  virtual ~AttributeExcludeHandler() {}

  virtual bool shouldExclude(Slice const& key, int nesting) = 0;
};

struct CustomTypeHandler {
  virtual ~CustomTypeHandler() {}

  virtual void dump(Slice const&, Dumper*, Slice const&) {
    throw Exception(Exception::NotImplemented);
  }

  virtual std::string toString(Slice const&, Options const*, Slice const&) {
    throw Exception(Exception::NotImplemented);
  }
                    
};

struct Options {
  enum UnsupportedTypeBehavior {
    NullifyUnsupportedType,
    ConvertUnsupportedType,
    FailOnUnsupportedType
  };

  Options() {}

  // Dumper behavior when a VPack value is serialized to JSON that
  // has no JSON equivalent
  UnsupportedTypeBehavior unsupportedTypeBehavior = FailOnUnsupportedType;

  // callback for excluding attributes from being built by the Parser
  AttributeExcludeHandler* attributeExcludeHandler = nullptr;

  AttributeTranslator* attributeTranslator = nullptr;

  // custom type handler used for processing custom types by Dumper and Slicer
  CustomTypeHandler* customTypeHandler = nullptr;

  // allow building Arrays without index table?
  bool buildUnindexedArrays = false;

  // allow building Objects without index table?
  bool buildUnindexedObjects = false;

  // pretty-print JSON output when dumping with Dumper
  bool prettyPrint = false;

  // keep top-level object/array open when building objects with the Parser
  bool keepTopLevelOpen = false;

  // clear builder before starting to parse in Parser
  bool clearBuilderBeforeParse = true;

  // validate UTF-8 strings when JSON-parsing with Parser
  bool validateUtf8Strings = false;

  // validate that attribute names in Object values are actually
  // unique when creating objects via Builder. This also includes
  // creation of Object values via a Parser
  bool checkAttributeUniqueness = false;

  // escape forward slashes when serializing VPack values into
  // JSON with a Dumper
  bool escapeForwardSlashes = false;

  // escape multi-byte Unicode characters when dumping them to JSON 
  // with a Dumper (creates \uxxxx sequences)
  bool escapeUnicode = false;

  // dump Object attributes in index order (true) or in "undefined"
  // order (false). undefined order may be faster but not deterministic
  bool dumpAttributesInIndexOrder = true;

  // disallow using type External (to prevent injection of arbitrary pointer
  // values as a security precaution)
  bool disallowExternals = false;

  // default options with the above settings
  static Options Defaults;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
