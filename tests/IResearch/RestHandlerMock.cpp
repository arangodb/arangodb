////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "RestHandlerMock.h"
#include "RestServer/VocbaseContext.h"
#include "VocBase/vocbase.h"

GeneralRequestMock::GeneralRequestMock(TRI_vocbase_t& vocbase) {
  _authenticated = false; // must be set before VocbaseContext::create(...)
  _isRequestContextOwner = false; // must be set before VocbaseContext::create(...)
  _context.reset(arangodb::VocbaseContext::create(*this, vocbase));
  _context->vocbase().forceUse(); // must be called or ~VocbaseContext() will fail at '_vocbase.release()'
  _requestContext = _context.get(); // do not use setRequestContext(...) since '_requestContext' has not been initialized and contains garbage
}

size_t GeneralRequestMock::contentLength() const {
  return _contentLength;
}

arangodb::StringRef GeneralRequestMock::rawPayload() const {
  return arangodb::StringRef(reinterpret_cast<const char*>(_payload.data()), _payload.size());
}

arangodb::velocypack::Slice GeneralRequestMock::payload(
    arangodb::velocypack::Options const* options /*= &arangodb::velocypack::Options::Defaults*/
) {
  return _payload.slice();
}

arangodb::Endpoint::TransportType GeneralRequestMock::transportType() {
  return arangodb::Endpoint::TransportType::HTTP; // arbitrary value
}

GeneralResponseMock::GeneralResponseMock(
    arangodb::ResponseCode code /*= arangodb::ResponseCode::OK*/
): arangodb::GeneralResponse(code) {
}

void GeneralResponseMock::addPayload(
    arangodb::velocypack::Buffer<uint8_t>&& buffer,
    arangodb::velocypack::Options const* options /*= nullptr*/,
    bool resolveExternals /*= true*/
) {
  addPayload(
    arangodb::velocypack::Builder(buffer).slice(), options, resolveExternals
  );
}

void GeneralResponseMock::addPayload(
    arangodb::velocypack::Slice const& slice,
    arangodb::velocypack::Options const* options /*= nullptr*/,
    bool resolveExternals /*= true*/
) {
  _payload = options
           ? arangodb::velocypack::Builder(slice, options)
           : arangodb::velocypack::Builder(slice)
           ;
}

void GeneralResponseMock::reset(arangodb::ResponseCode code) {
  _headers.clear();
  _payload.clear();
  _responseCode = code;
}

arangodb::Endpoint::TransportType GeneralResponseMock::transportType() {
  return arangodb::Endpoint::TransportType::HTTP; // arbitrary value
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
