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

namespace arangodb {
class RestBatchHandler;

namespace rest {
class GeneralCommTask;
}  // namespace rest

class HttpResponse : public GeneralResponse {
//  friend class rest::HttpCommTask;
  friend class rest::GeneralCommTask;
  friend class RestBatchHandler;  // TODO must be removed

 public:
  static bool HIDE_PRODUCT_HEADER;

 public:
  explicit HttpResponse(ResponseCode code, basics::StringBuffer* leased);
  ~HttpResponse();

 public:
  bool isHeadResponse() const { return _isHeadResponse; }

 public:
  void setCookie(std::string const& name, std::string const& value,
                 int lifeTimeSeconds, std::string const& path,
                 std::string const& domain, bool secure, bool httpOnly);

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

  // you should call writeHeader only after the body has been created
  void writeHeader(basics::StringBuffer*);  // override;

 public:
  void reset(ResponseCode code) override final;

  void addPayload(velocypack::Slice const&,
                  velocypack::Options const* = nullptr,
                  bool resolve_externals = true) override;
  void addPayload(velocypack::Buffer<uint8_t>&&,
                  velocypack::Options const* = nullptr,
                  bool resolve_externals = true) override;

  /// used for head-responses
  bool setGenerateBody(bool generateBody) override final {
    return _generateBody = generateBody;
  }

  int reservePayload(std::size_t size) override { return _body->reserve(size); }

  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::HTTP;
  }

 private:
  // the body must already be set. deflate is then run on the existing body
  int deflate(size_t = 16384);

  std::unique_ptr<basics::StringBuffer> stealBody() {
    std::unique_ptr<basics::StringBuffer> bb(_body);
    _body = nullptr;
    return bb;
  }

 private:
  bool _isHeadResponse;
  std::vector<std::string> _cookies;
  basics::StringBuffer* _body;
  size_t _bodySize;

  void addPayloadInternal(velocypack::Slice, size_t, velocypack::Options const*, bool);
};
}  // namespace arangodb

#endif
