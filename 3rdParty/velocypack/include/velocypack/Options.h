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

#include "velocypack/velocypack-common.h"

namespace arangodb {
namespace velocypack {
class AttributeTranslator;
class Dumper;
struct Options;
class Slice;

struct CustomTypeHandler {
  virtual ~CustomTypeHandler() = default;
  virtual void dump(Slice const&, Dumper*, Slice const&);
  virtual std::string toString(Slice const&, Options const*, Slice const&);
};

struct Options {
  // Behavior to be applied when dumping VelocyPack values that cannot be
  // expressed in JSON without data loss
  enum UnsupportedTypeBehavior {
    // convert any non-JSON-representable value to null
    NullifyUnsupportedType,
    // emit a JSON string "(non-representable type ...)"
    ConvertUnsupportedType,
    // throw an exception for any non-JSON-representable value
    FailOnUnsupportedType
  };

  // Behavior to be applied when building VelocyPack Array/Object values
  // with a Builder
  enum PaddingBehavior {
    // use padding - fill unused head bytes with zero-bytes (ASCII NUL) in
    // order to avoid a later memmove
    UsePadding,
    // don't pad and do not fill any gaps with zero-bytes (ASCII NUL). 
    // instead, memmove data down so there is no gap between the head bytes
    // and the payload
    NoPadding,
    // pad in cases the Builder considers it useful, and don't pad in other
    // cases when the Builder doesn't consider it useful
    Flexible
  };

  Options() {}

  // Dumper behavior when a VPack value is serialized to JSON that
  // has no JSON equivalent
  UnsupportedTypeBehavior unsupportedTypeBehavior = FailOnUnsupportedType;
 
  // Builder behavior w.r.t. padding or memmoving data
  PaddingBehavior paddingBehavior = PaddingBehavior::Flexible;

  // custom attribute translator for integer keys
  AttributeTranslator* attributeTranslator = nullptr;

  // custom type handler used for processing custom types by Dumper and Slicer
  CustomTypeHandler* customTypeHandler = nullptr;

  // allow building Arrays without index table?
  bool buildUnindexedArrays = false;

  // allow building Objects without index table?
  bool buildUnindexedObjects = false;

  // pretty-print JSON output when dumping with Dumper
  bool prettyPrint = false;

  // pretty-print JSON output when dumping with Dumper, but don't add any newlines
  bool singleLinePrettyPrint = false;

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

  // dump NaN as "NaN", Infinity as "Infinity"
  bool unsupportedDoublesAsString = false;

  // dump binary values as hex-encoded strings
  bool binaryAsHex = false;

  // render dates as integers
  bool datesAsIntegers = false;

  // disallow using type External (to prevent injection of arbitrary pointer
  // values as a security precaution), validated when object-building via
  // Builder and VelocyPack validation using Validator objects
  bool disallowExternals = false;
  
  // disallow using type Custom (to prevent injection of arbitrary opaque
  // values as a security precaution)
  bool disallowCustom = false;
  
  // disallow tagged values
  bool disallowTags = false;
  
  // disallow BCD values
  bool disallowBCD = false;

  // write tags to JSON output
  bool debugTags = false;

  // default options with the above settings
  static Options Defaults;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
