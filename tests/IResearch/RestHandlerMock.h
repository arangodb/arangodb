////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__REST_HANDLER_MOCK_H
#define ARANGODB_IRESEARCH__REST_HANDLER_MOCK_H 1

#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"

struct TRI_vocbase_t; // forward declaration

namespace arangodb {
class VocbaseContext; // forward declaration
}

struct GeneralRequestMock: public arangodb::GeneralRequest {
  int64_t _contentLength;
  std::unique_ptr<arangodb::VocbaseContext> _context; // VocbaseContext required for use with RestVocbaseBaseHandler
  arangodb::velocypack::Builder _payload; // request body

  GeneralRequestMock(TRI_vocbase_t& vocbase);
  ~GeneralRequestMock();
  using arangodb::GeneralRequest::addSuffix;
  virtual size_t contentLength() const override;
  virtual void setDefaultContentType() override {
    _contentType = arangodb::rest::ContentType::VPACK;
  }
  virtual arangodb::velocypack::StringRef rawPayload() const override;
  virtual arangodb::velocypack::Slice payload(bool strictValidation = true) override;
  virtual void setPayload(arangodb::velocypack::Buffer<uint8_t> buffer) override;
  virtual arangodb::Endpoint::TransportType transportType() override;
  std::unordered_map<std::string, std::string>& values() { return _values; }
};

struct GeneralResponseMock: public arangodb::GeneralResponse {
  arangodb::velocypack::Builder _payload;
  virtual bool isResponseEmpty() const override {
    return _payload.isEmpty();
  }

  GeneralResponseMock(arangodb::ResponseCode code = arangodb::ResponseCode::OK);
  virtual void addPayload(arangodb::velocypack::Buffer<uint8_t>&& buffer, arangodb::velocypack::Options const* options = nullptr, bool resolveExternals = true) override;
  virtual void addPayload(arangodb::velocypack::Slice const& slice, arangodb::velocypack::Options const* options = nullptr, bool resolveExternals = true) override;
  virtual void addRawPayload(arangodb::velocypack::StringRef payload) override;
  virtual void reset(arangodb::ResponseCode code) override;
  virtual arangodb::Endpoint::TransportType transportType() override;
  int deflate(size_t size = 16384) override { return 0; }
  bool isCompressionAllowed() override { return false; }
};

#endif
