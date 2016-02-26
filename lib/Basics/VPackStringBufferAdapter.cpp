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

#include "VPackStringBufferAdapter.h"
#include "Basics/Exceptions.h"

void arangodb::basics::VPackStringBufferAdapter::push_back(char c) {
  int res = TRI_AppendCharStringBuffer(_buffer, c);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}
 
void arangodb::basics::VPackStringBufferAdapter::append(std::string const& p) {
  int res = TRI_AppendString2StringBuffer(_buffer, p.c_str(), p.size());
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void arangodb::basics::VPackStringBufferAdapter::append(char const* p) {
  int res = TRI_AppendString2StringBuffer(_buffer, p, strlen(p));
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void arangodb::basics::VPackStringBufferAdapter::append(char const* p,
                                                        uint64_t len) {
  int res = TRI_AppendString2StringBuffer(_buffer, p, static_cast<size_t>(len));
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void arangodb::basics::VPackStringBufferAdapter::reserve(uint64_t len) {
  int res = TRI_ReserveStringBuffer(_buffer, static_cast<size_t>(len));
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}
