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

#include "TraverserEngine.h"

#include "Basics/Exceptions.h"
#include "VocBase/Traverser.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::traverser;

static const std::string OPTIONS = "options";
static const std::string SHARDS = "shards";
static const std::string EDGES = "edges";

TraverserEngine::TraverserEngine(arangodb::velocypack::Slice info) : _opts(nullptr) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (optsSlice.isNone() || !optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires a " + OPTIONS + " attribute.");
  }

  VPackSlice shardsSlice = info.get(SHARDS);
  if (shardsSlice.isNone() || !shardsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires a " + SHARDS + " attribute.");
  }

  VPackSlice edgesSlice = shardsSlice.get(EDGES);

  if (shardsSlice.isNone() || !shardsSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + SHARDS + " object requires an " + EDGES + " attribute.");
  }

  // TODO create the transaction here with partly locked collections
  // ASK MAX!
  arangodb::Transaction* trx = nullptr;
  _opts.reset(new TraverserOptions(trx, optsSlice, edgesSlice));
}

TraverserEngine::~TraverserEngine() {
}
