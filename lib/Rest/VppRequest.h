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

#ifndef ARANGODB_REST_VPP_REQUEST_H
#define ARANGODB_REST_VPP_REQUEST_H 1

#include "Rest/GeneralRequest.h"
#include "Endpoint/ConnectionInfo.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
class RestBatchHandler;

namespace rest {
class GeneralCommTask;
class VppCommTask;
// class VppsCommTask;
}

namespace velocypack {
class Builder;
struct Options;
}

class VppRequest : public GeneralRequest {
  friend class rest::VppCommTask;
  // friend class rest::VppsCommTask;
  friend class rest::GeneralCommTask;
  friend class RestBatchHandler;  // TODO must be removed

 private:
  VppRequest(ConnectionInfo const& connectionInfo,
             VPackBuffer<uint8_t>&& header, size_t length);

 private:
  static std::string const _bla;
  static std::unordered_map<std::string, std::string> _blam;

 public:
  ~VppRequest() {}

 public:
  VPackSlice payload(arangodb::velocypack::Options const*) override;
  int64_t contentLength() const override { return _contentLength; }
  void setPayload(VPackBuffer<uint8_t>&& payload) { _payload = payload; }

  std::unordered_map<std::string, std::string> const& headers() const override {
    throw std::logic_error("not implemented");
    return _blam;
  }

  void setHeaders(std::unordered_map<std::string, std::string> const& headers) {
    throw std::logic_error("the base class has no headers anymore.");
    // add a new ctor to HttpRequest that is cheaper in order to wrap simple
    // http request
    // and remove The Fake request
    //_headers = headers;  // this is from the base class
  }

  // get value from headers map. The key must be lowercase.
  std::string const& header(std::string const& key) const override {
    throw std::logic_error("not implemented");
    return _bla;
  }
  std::string const& header(std::string const& key,
                            bool& found) const override {
    throw std::logic_error("not implemented");
    return _bla;
  }

  virtual std::string const& value(std::string const& key) const override {
    throw std::logic_error("not implemented");
    return _bla;
  }
  virtual std::string const& value(std::string const& key,
                                   bool& found) const override {
    throw std::logic_error("not implemented");
    return _bla;
  }
  virtual std::unordered_map<std::string, std::string> values() const override {
    throw std::logic_error("not implemented");
    return std::unordered_map<std::string, std::string>();
  }
  virtual std::unordered_map<std::string, std::vector<std::string>>
  arrayValues() const override {
    throw std::logic_error("not implemented");
    return std::unordered_map<std::string, std::vector<std::string>>();
  }

 private:
  void parseHeader();  // converts _header(vpack) to
                       // _headers(map<string,string>)

  int64_t _contentLength;
  VPackBuffer<uint8_t> _header;
  VPackBuffer<uint8_t> _payload;
  const std::unordered_map<std::string, std::string> _cookies;  // TODO remove
};
}
#endif
