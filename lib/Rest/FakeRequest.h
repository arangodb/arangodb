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

#ifndef ARANGODB_REST_FAKE_REQUEST_H
#define ARANGODB_REST_FAKE_REQUEST_H 1

#include "Basics/Common.h"

#include "Endpoint/ConnectionInfo.h"
#include "Rest/GeneralRequest.h"

namespace arangodb {

class FakeRequest : public GeneralRequest {
  // This class is necessary to fake a GeneralRequest, which is a pure virtual
  // base class. We need this because ClusterComm::syncRequest returns a
  // HttpResponse whereas ClusterComm::asyncRequest returns a GeneralRequest
  // from the request in the backward direction.

 public:
  FakeRequest(ContentType contentType, char const* body, int64_t contentLength);

  ~FakeRequest();

  // the content length
  int64_t contentLength() const override { return _contentLength; }

  std::unordered_map<std::string, std::string> cookieValues() const override {
    return _cookies;
  }

  // Payload
  velocypack::Slice payload(arangodb::velocypack::Options const*) override final;

  void setHeaders(std::unordered_map<std::string, std::string> const& headers) {
    _headers = headers;  // this is from the base class
  }

 private:
  std::unordered_map<std::string, std::string> _cookies;
  ContentType _contentType;
  char const* _body;
  int64_t _contentLength;
  std::shared_ptr<velocypack::Builder> _vpackBuilder;
};
}

#endif
