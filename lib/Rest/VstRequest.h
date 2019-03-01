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
}  // namespace rest

namespace velocypack {
class Builder;
struct Options;
}  // namespace velocypack

using rest::VstInputMessage;

class VstRequest final : public GeneralRequest {
  friend class rest::VstCommTask;
  friend class rest::GeneralCommTask;

 public:
  VstRequest(ConnectionInfo const& connectionInfo, VstInputMessage&& message,
             uint64_t messageId);

  ~VstRequest() {}

 public:
  uint64_t messageId() const override { return _messageId; }

  size_t contentLength() const override { return _message.payloadSize(); }
  arangodb::velocypack::StringRef rawPayload() const override { return _message.payload(); }
  VPackSlice payload(arangodb::velocypack::Options const*) override;

  virtual arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::VST;
  };

 private:
  void setHeader(VPackSlice key, VPackSlice content);

  void parseHeaderInformation();

 private:
  uint64_t _messageId;
  VstInputMessage _message;

  /// @brief was VPack payload validated
  bool _validatedPayload;
  /// @brief if payload was not VPack this will store parsed result
  std::shared_ptr<velocypack::Builder> _vpackBuilder;
};
}  // namespace arangodb
#endif
