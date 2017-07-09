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

#ifndef ARANGODB_REST_VST_REQUEST_H
#define ARANGODB_REST_VST_REQUEST_H 1

#include "Endpoint/ConnectionInfo.h"
#include "Rest/GeneralRequest.h"
#include "Rest/VstMessage.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
class RestBatchHandler;

namespace rest {
class GeneralCommTask;
class VstCommTask;
}

namespace velocypack {
class Builder;
struct Options;
}

using rest::VstInputMessage;

class VstRequest final : public GeneralRequest {
  friend class rest::VstCommTask;
  friend class rest::GeneralCommTask;
  friend class RestBatchHandler;  // TODO must be removed

 private:
  VstRequest(ConnectionInfo const& connectionInfo, VstInputMessage&& message,
             uint64_t messageId, bool isFake = false);

 public:
  ~VstRequest() {}

 public:
  uint64_t messageId() const override { return _messageId; }
  VPackSlice payload(arangodb::velocypack::Options const*) override;

  int64_t contentLength() const override {
    return _message.payload().byteSize();  // Fixme for MultiPayload message
  }

  virtual arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::VST;
  };

  std::unordered_map<std::string, std::string> const& headers() const override;
  // get value from headers map. The key must be lowercase.
  std::string const& header(std::string const& key) const override;
  std::string const& header(std::string const& key, bool& found) const override;

  // values are query paramteres
  std::unordered_map<std::string, std::string> values() const override {
    return _values;
  }
  std::unordered_map<std::string, std::vector<std::string>> arrayValues()
      const override {
    return _arrayValues;
  }
  std::string const& value(std::string const& key) const override;
  std::string const& value(std::string const& key, bool& found) const override;

 private:
  VstInputMessage _message;
  mutable std::unique_ptr<std::unordered_map<std::string, std::string>>
      _headers;
  // values are query parameters
  std::unordered_map<std::string, std::string> _values;
  std::unordered_map<std::string, std::vector<std::string>> _arrayValues;
  uint64_t _messageId;

  void parseHeaderInformation();
};
}
#endif
