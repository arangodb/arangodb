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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_EXAMPLE_MATCHER_H
#define ARANGOD_VOC_BASE_EXAMPLE_MATCHER_H 1

#include "Utils/CollectionNameResolver.h"
#include "VocBase/document-collection.h"

#include <v8.h>

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}

class ExampleMatcher {
 private:
  struct ExampleDefinition {
    std::vector<std::vector<std::string>> _paths;
    arangodb::velocypack::Builder _values;

    arangodb::velocypack::Slice slice() const;
  };

  std::vector<ExampleDefinition> definitions;

  void fillExampleDefinition(arangodb::velocypack::Slice const& example,
                             ExampleDefinition& def);

  void fillExampleDefinition(v8::Isolate* isolate,
                             v8::Handle<v8::Object> const& example,
                             v8::Handle<v8::Array> const& names, size_t& n,
                             std::string& errorMessage, ExampleDefinition& def);

 public:
  ExampleMatcher(v8::Isolate* isolate, v8::Handle<v8::Object> const example,
                 std::string& errorMessage);

  ExampleMatcher(v8::Isolate* isolate, v8::Handle<v8::Array> const examples,
                 std::string& errorMessage);

  ExampleMatcher(TRI_json_t const* example,
                 arangodb::CollectionNameResolver const* resolver);

  ExampleMatcher(arangodb::velocypack::Slice const& example,
                 bool allowStrings);

  ~ExampleMatcher() { }

  bool matches(arangodb::velocypack::Slice const) const;
};
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|//
// --SECTION--\\|/// @\\}"
