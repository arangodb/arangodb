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

#include "Rest/GeneralRequest.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

namespace velocypack {
class Builder;
struct Options;
}  // namespace velocypack

class VstRequest final : public GeneralRequest {
  
 public:
  VstRequest(ConnectionInfo const& connectionInfo,
             velocypack::Buffer<uint8_t> buffer,
             size_t payloadOffset,
             uint64_t messageId);

  ~VstRequest() {}

 public:
  
  uint64_t messageId() const override { return _messageId; }
  size_t contentLength() const override { return _buffer.size(); }
  arangodb::velocypack::StringRef rawPayload() const override {
    return arangodb::velocypack::StringRef(reinterpret_cast<const char*>(_buffer.data()),
                                           _buffer.size());
  }
  velocypack::Slice payload(arangodb::velocypack::Options const*) override;

  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::VST;
  };

 private:
  void setHeader(velocypack::Slice key, velocypack::Slice content);

  void parseHeaderInformation();

 private:
  velocypack::Buffer<uint8_t> const _buffer;
  /// @brief if payload was not VPack this will store parsed result
  std::shared_ptr<velocypack::Builder> _vpackBuilder;
  size_t const _payloadOffset;
  uint64_t const _messageId;
  /// @brief was VPack payload validated
  bool _validatedPayload;
};
}  // namespace arangodb
#endif
