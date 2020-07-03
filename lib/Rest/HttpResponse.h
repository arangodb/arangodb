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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_HTTP_RESPONSE_H
#define ARANGODB_REST_HTTP_RESPONSE_H 1

#include "Rest/GeneralResponse.h"

#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"

namespace arangodb {
class RestBatchHandler;

class HttpResponse : public GeneralResponse {
  friend class RestBatchHandler;  // TODO must be removed

 public:
  static bool HIDE_PRODUCT_HEADER;

 public:
  HttpResponse(ResponseCode code, uint64_t mid,
               std::unique_ptr<basics::StringBuffer> = nullptr);
  ~HttpResponse() = default;

 public:
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
  size_t bodySize() const;

  void sealBody() {
    _bodySize = _body->length();
  }

  // you should call writeHeader only after the body has been created
  void writeHeader(basics::StringBuffer*);  // override;

 public:
  void reset(ResponseCode code) override final;

  void addPayload(velocypack::Slice const&,
                  velocypack::Options const* = nullptr,
                  bool resolve_externals = true) override final;
  void addPayload(velocypack::Buffer<uint8_t>&&,
                  velocypack::Options const* = nullptr,
                  bool resolve_externals = true) override final;
  void addRawPayload(velocypack::StringRef payload) override final;

  bool isResponseEmpty() const override final {
    return _body->empty();
  }

  int reservePayload(std::size_t size) override final { return _body->reserve(size); }

  arangodb::Endpoint::TransportType transportType() override final {
    return arangodb::Endpoint::TransportType::HTTP;
  }

  std::unique_ptr<basics::StringBuffer> stealBody() {
    std::unique_ptr<basics::StringBuffer> body(std::move(_body));
    return body;
  }
  
 private:
  // the body must already be set. deflate is then run on the existing body
  int deflate(size_t size = 16384) override {
    return _body->deflate(size);
  }

  void addPayloadInternal(velocypack::Slice, size_t, velocypack::Options const*, bool);
  
 private:
  std::vector<std::string> _cookies;
  std::unique_ptr<basics::StringBuffer> _body;
  size_t _bodySize;
};
}  // namespace arangodb

#endif
