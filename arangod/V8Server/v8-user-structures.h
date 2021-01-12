////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_V8_SERVER_V8_USER_STRUCTURES_H
#define ARANGOD_V8_SERVER_V8_USER_STRUCTURES_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

#include "V8/v8-globals.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace arangodb {

namespace velocypack {
template <typename T> class Buffer;
}

class CacheKeySpace {
 public:
  v8::Handle<v8::Value> keyGet(v8::Isolate* isolate, std::string const& key);
  bool keySet(v8::Isolate* isolate, std::string const& key,
              v8::Handle<v8::Value> const& value, bool replace);
 private:
  arangodb::basics::ReadWriteLock _lock;
  std::unordered_map<std::string, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>> _hash;
};

struct DatabaseJavaScriptCache {
  ~DatabaseJavaScriptCache();

  arangodb::basics::ReadWriteLock lock;
  std::unordered_map<std::string, std::unique_ptr<CacheKeySpace>> keyspaces;
};

} // namespace arangodb

struct TRI_vocbase_t;

/// @brief creates the user structures functions
void TRI_InitV8UserStructures(v8::Isolate* isolate, v8::Handle<v8::Context>);

#endif
