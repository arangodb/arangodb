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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "EnumerateViewBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

EnumerateViewBlock::EnumerateViewBlock(ExecutionEngine* engine,
                                       EnumerateViewNode const* en)
    : ExecutionBlock(engine, en),
      _view(nullptr) // TODO
       {
  // TODO: set iterator
}

EnumerateViewBlock::~EnumerateViewBlock() {}

int EnumerateViewBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // TODO: handle local data (if any)

  return TRI_ERROR_NO_ERROR;
  DEBUG_END_BLOCK();
}

AqlItemBlock* EnumerateViewBlock::getSome(size_t, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();
  if (_done) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  // TODO: actually get some

  traceGetSomeEnd(nullptr);
  return nullptr;
  DEBUG_END_BLOCK();
}

size_t EnumerateViewBlock::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  if (_done) {
    return 0;
  }

  size_t skipped = 0;

  // TODO: actually skip some

  return skipped;
  DEBUG_END_BLOCK();
}
