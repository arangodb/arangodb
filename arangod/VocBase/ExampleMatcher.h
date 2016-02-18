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

#include "v8.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/document-collection.h"

struct TRI_doc_mptr_t;
class VocShaper;

namespace arangodb {

namespace velocypack {
class Slice;
}

class ExampleMatcher {
  struct DocumentId {
    TRI_voc_cid_t cid;
    std::string key;

    DocumentId(TRI_voc_cid_t cid, std::string const& key)
        : cid(cid), key(key) {}
  };

  enum internalAttr { key, id, rev, from, to };

  // Has no destructor.
  // The using ExampleMatcher will free all pointers.
  // Should not directly be used from outside.
  struct ExampleDefinition {
    std::map<internalAttr, DocumentId> _internal;
    std::vector<TRI_shape_pid_t> _pids;
    std::vector<TRI_shaped_json_t*> _values;
  };

  VocShaper* _shaper;
  std::vector<ExampleDefinition> definitions;

  void fillExampleDefinition(TRI_json_t const* example,
                             arangodb::CollectionNameResolver const* resolver,
                             ExampleDefinition& def);

  void fillExampleDefinition(arangodb::velocypack::Slice const& example,
                             arangodb::CollectionNameResolver const* resolver,
                             ExampleDefinition& def);

  void fillExampleDefinition(v8::Isolate* isolate,
                             v8::Handle<v8::Object> const& example,
                             v8::Handle<v8::Array> const& names, size_t& n,
                             std::string& errorMessage, ExampleDefinition& def);

 public:
  ExampleMatcher(v8::Isolate* isolate, v8::Handle<v8::Object> const example,
                 VocShaper* shaper, std::string& errorMessage);

  ExampleMatcher(v8::Isolate* isolate, v8::Handle<v8::Array> const examples,
                 VocShaper* shaper, std::string& errorMessage);

  ExampleMatcher(TRI_json_t const* example, VocShaper* shaper,
                 arangodb::CollectionNameResolver const* resolver);

  ExampleMatcher(arangodb::velocypack::Slice const& example, VocShaper* shaper,
                 arangodb::CollectionNameResolver const* resolver,
                 bool allowStrings);

  ~ExampleMatcher() { cleanup(); }

  bool matches(TRI_voc_cid_t cid, TRI_doc_mptr_t const* mptr) const;

 private:
  void cleanup();
};
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|//
// --SECTION--\\|/// @\\}"
