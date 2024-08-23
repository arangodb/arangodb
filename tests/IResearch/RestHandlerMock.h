////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
class VocbaseContext;
}

struct GeneralRequestMock : public arangodb::GeneralRequest {
  int64_t _contentLength;
  std::shared_ptr<arangodb::VocbaseContext>
      _context;  // VocbaseContext required for use with RestVocbaseBaseHandler
  arangodb::velocypack::Builder _payload;  // request body

  GeneralRequestMock(TRI_vocbase_t& vocbase);
  ~GeneralRequestMock();
  using arangodb::GeneralRequest::addSuffix;
  size_t contentLength() const noexcept override;
  void setDefaultContentType() noexcept override {
    _contentType = arangodb::rest::ContentType::VPACK;
  }
  std::string_view rawPayload() const override;
  arangodb::velocypack::Slice payload(bool strictValidation = true) override;
  void setPayload(arangodb::velocypack::Buffer<uint8_t> buffer) override;
  void setData(arangodb::velocypack::Slice slice);
  std::unordered_map<std::string, std::string>& values() { return _values; }
};

struct GeneralResponseMock : public arangodb::GeneralResponse {
  arangodb::velocypack::Builder _payload;
  virtual bool isResponseEmpty() const noexcept override {
    return _payload.isEmpty();
  }

  GeneralResponseMock(arangodb::ResponseCode code = arangodb::ResponseCode::OK);
  virtual void addPayload(
      arangodb::velocypack::Buffer<uint8_t>&& buffer,
      arangodb::velocypack::Options const* options = nullptr,
      bool resolveExternals = true) override;
  virtual void addPayload(
      arangodb::velocypack::Slice slice,
      arangodb::velocypack::Options const* options = nullptr,
      bool resolveExternals = true) override;
  virtual void addRawPayload(std::string_view payload) override;
  virtual void reset(arangodb::ResponseCode code) override;
  void setAllowCompression(
      arangodb::rest::ResponseCompressionType rct) noexcept override {}
  arangodb::rest::ResponseCompressionType compressionAllowed()
      const noexcept override {
    return arangodb::rest::ResponseCompressionType::kNoCompression;
  }
  virtual size_t bodySize() const override { return _payload.size(); }
  virtual ErrorCode zlibDeflate(bool onlyIfSmaller) override;
  virtual ErrorCode gzipCompress(bool onlyIfSmaller) override;
  virtual ErrorCode lz4Compress(bool onlyIfSmaller) override;
  void clearBody() noexcept override { _payload.clear(); }
};
