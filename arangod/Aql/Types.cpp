////////////////////////////////////////////////////////////////////////////////
/// @brief fundamental types for the optimisation and execution of AQL
///
/// @file arangod/Aql/Types.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Types.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

AqlValue::~AqlValue () {
  switch (_type) {
    case JSON: {
      delete _json;
      return;
    }
    case DOCVEC: {
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        delete *it;
      }
      delete _vector;
      return;
    }
    case RANGE: {
      return;
    }
    default:
      return;
  }
}

AqlValue* AqlValue::clone () const {
  switch (_type) {
    case JSON: {
      return new AqlValue(new Json(_json->copy()));
    }
    case DOCVEC: {
      auto c = new vector<AqlItemBlock*>;
      c->reserve(_vector->size());
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        c->push_back((*it)->slice(0, (*it)->size()));
      }
      return new AqlValue(c);
    }
    case RANGE: {
      return new AqlValue(_range._low, _range._high);
    }
    default:
      return nullptr;
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief splice multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* AqlItemBlock::splice(std::vector<AqlItemBlock*>& blocks)
{
  return nullptr;
}

