////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt <andreas@arangodb.com>
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_COMMUNICATOR_RESULT_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_COMMUNICATOR_RESULT_H 1

#include "SimpleHttpClient/SimpleHttpResult.h"

namespace arangodb {
namespace httpclient {

class SimpleHttpCommunicatorResult: public SimpleHttpResult {
  using SimpleHttpResult::getBody;
  using SimpleHttpResult::getBodyVelocyPack;

  public:
    SimpleHttpCommunicatorResult() = delete;
    explicit SimpleHttpCommunicatorResult(HttpResponse* response)
      : SimpleHttpResult(), _response(response), _headers(nullptr) {
    }
    virtual ~SimpleHttpCommunicatorResult() {}

  public:
    virtual bool wasHttpError() const override { return getHttpReturnCode() >= 400; }
    virtual int getHttpReturnCode() const override { return static_cast<int>(_response->responseCode()); }
    virtual std::string getHttpReturnMessage() const override { return GeneralResponse::responseString(_response->responseCode()); }
    virtual bool hasContentLength() const override { return true; }
    virtual size_t getContentLength() const override { return _response->body().length(); }
    arangodb::basics::StringBuffer& getBody() override { return _response->body(); }
    std::shared_ptr<VPackBuilder> getBodyVelocyPack(VPackOptions const& options) const override {
      return VPackParser::fromJson(_response->body().c_str(), _response->body().length(), &options);
    }
    virtual enum resultTypes getResultType() const override {
      std::string message(__func__);
      message += " not implemented";
      throw std::runtime_error(message);
    }
    virtual bool isComplete() const override { return true; }
    virtual bool isChunked() const override { return false; }
    virtual bool isDeflated() const override {
      std::string message(__func__);
      message += " not implemented";
      throw std::runtime_error(message);
    }
    virtual std::string getResultTypeMessage() const override {
      std::string message(__func__);
      message += " not implemented";
      throw std::runtime_error(message);
    }
    virtual void addHeaderField(char const*, size_t) override {
      std::string message(__func__);
      message += " not implemented";
      throw std::runtime_error(message);
    }
    virtual std::string getHeaderField(std::string const& header, bool& found) const override {
      auto const& headers = _response->headers();
      auto it = headers.find(header);
      if (it == headers.end()) {
        return "";
      }
      found = true;
      return it->second;
    }
    virtual bool hasHeaderField(std::string const&) const override {
      std::string message(__func__);
      message += " not implemented";
      throw std::runtime_error(message);
    }
    virtual std::unordered_map<std::string, std::string> const& getHeaderFields() const override {
      if (_headers == nullptr) {
        _headers.reset(new std::unordered_map<std::string, std::string>(_response->headers()));
      }
      return *_headers.get();
    }
    virtual bool isJson() const override {
      std::string message(__func__);
      message += " not implemented";
      throw std::runtime_error(message);
    }
    
 
  private:
    std::unique_ptr<HttpResponse> _response;
    mutable std::unique_ptr<std::unordered_map<std::string, std::string>> _headers;
};
}
}

#endif
