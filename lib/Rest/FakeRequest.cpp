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

#include "Rest/FakeRequest.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

FakeRequest::FakeRequest(ContentType contentType, char const* body,
                         int64_t contentLength)
  : GeneralRequest(ConnectionInfo()),
   _contentType(contentType),
   _body(body),
   _contentLength(contentLength) {
}

FakeRequest::~FakeRequest() {
}

// Payload
VPackSlice FakeRequest::payload(arangodb::velocypack::Options const* options) {
  TRI_ASSERT(_vpackBuilder == nullptr);
  // check options for nullptr?

  if( _contentType == ContentType::JSON) {
    VPackParser parser(options);
    if (_contentLength > 0) {
      parser.parse(_body, static_cast<size_t>(_contentLength));
    }
    _vpackBuilder = parser.steal();
    return VPackSlice(_vpackBuilder->slice());
  } else /*VPACK*/{
    VPackValidator validator;
    validator.validate(_body, static_cast<size_t>(_contentLength));
    return VPackSlice(reinterpret_cast<uint8_t const*>(_body));
  }
}

