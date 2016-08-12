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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_VPP_RESPONSE_H
#define ARANGODB_REST_VPP_RESPONSE_H 1

#include "Rest/VppMessage.h"
#include "Rest/GeneralResponse.h"
#include "Basics/StringBuffer.h"

namespace arangodb {
class RestBatchHandler;

namespace rest {
class VppCommTask;
class GeneralCommTask;
}

using rest::VPackMessageNoOwnBuffer;

class VppResponse : public GeneralResponse {
  friend class rest::GeneralCommTask;
  friend class rest::VppCommTask;
  friend class RestBatchHandler;  // TODO must be removed

  VppResponse(ResponseCode code, uint64_t id);

 public:
  static bool HIDE_PRODUCT_HEADER;

  // required by base
  void reset(ResponseCode code) final;
  void setPayload(ContentType contentType, arangodb::velocypack::Slice const&,
                  bool generateBody,
                  arangodb::velocypack::Options const&) final;
  virtual arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::VPP;
  };

  VPackMessageNoOwnBuffer prepareForNetwork();

 private:
  //_responseCode   - from Base
  //_headers        - from Base
  std::shared_ptr<VPackBuffer<uint8_t>>
      _header;  // generated form _headers when prepared for network
  VPackBuffer<uint8_t> _payload;
  uint64_t _messageID;
  bool _generateBody;  // this must be true if payload should be send
};
}

#endif
