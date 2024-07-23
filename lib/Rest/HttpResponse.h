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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Rest/GeneralResponse.h"

#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"

#include <memory>
#include <string>
#include <vector>

namespace arangodb {
class RestBatchHandler;

class HttpResponse : public GeneralResponse {
  friend class RestBatchHandler;  // TODO must be removed

 public:
  HttpResponse(ResponseCode code, uint64_t mid,
               std::unique_ptr<basics::StringBuffer> buffer,
               rest::ResponseCompressionType rct);
  ~HttpResponse() = default;

  void setCookie(std::string const& name, std::string const& value,
                 int lifeTimeSeconds, std::string const& path,
                 std::string const& domain, bool secure, bool httpOnly);
  std::vector<std::string> const& cookies() const { return _cookies; }

  // In case of HEAD request, no body must be defined. However, the response
  // needs to know the size of body.
  void headResponse(size_t);

  // Returns a reference to the body. This reference is only valid as long as
  // http response exists. You can add data to the body by appending
  // information to the string buffer. Note that adding data to the body
  // invalidates any previously returned header. You must call header
  // again.
  basics::StringBuffer& body() {
    TRI_ASSERT(_body);
    return *_body;
  }

  size_t bodySize() const override;

  void sealBody() { _bodySize = _body->length(); }

  // you should call writeHeader only after the body has been created
  void writeHeader(basics::StringBuffer*);  // override;

  void clearBody() noexcept override;

  void reset(ResponseCode code) override final;

  void addPayload(velocypack::Slice slice, velocypack::Options const* = nullptr,
                  bool resolve_externals = true) override final;
  void addPayload(velocypack::Buffer<uint8_t>&&,
                  velocypack::Options const* = nullptr,
                  bool resolve_externals = true) override final;
  void addRawPayload(std::string_view payload) override final;

  void setAllowCompression(
      rest::ResponseCompressionType rct) noexcept override final;

  rest::ResponseCompressionType compressionAllowed()
      const noexcept override final;

  bool isResponseEmpty() const noexcept override final {
    return _body->empty();
  }

  ErrorCode reservePayload(std::size_t size) override final {
    return _body->reserve(size);
  }

  std::unique_ptr<basics::StringBuffer> stealBody() {
    std::unique_ptr<basics::StringBuffer> body(std::move(_body));
    return body;
  }

 private:
  // the body must already be set. deflate is then run on the existing body
  ErrorCode zlibDeflate(bool onlyIfSmaller) override;

  // the body must already be set. gzip compression is then run on the existing
  // body
  ErrorCode gzipCompress(bool onlyIfSmaller) override;

  // the body must already be set. lz4 compression is then run on the existing
  // body
  ErrorCode lz4Compress(bool onlyIfSmaller) override;

  void addPayloadInternal(uint8_t const* data, size_t length,
                          velocypack::Options const* options,
                          bool resolveExternals);

  std::vector<std::string> _cookies;
  std::unique_ptr<basics::StringBuffer> _body;
  size_t _bodySize;
  rest::ResponseCompressionType _allowCompression;
};
}  // namespace arangodb
