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

#include "CollectionKeys.h"
#include "VocBase/ticks.h"

using namespace arangodb;

CollectionKeys::CollectionKeys(TRI_vocbase_t* vocbase, double ttl)
    : _vocbase(vocbase),
      _collection(nullptr),
      _id(0),
      _ttl(ttl),
      _expires(0.0),
      _isDeleted(false),
      _isUsed(false) {
  _id = TRI_NewTickServer();
  _expires = TRI_microtime() + _ttl;
}
