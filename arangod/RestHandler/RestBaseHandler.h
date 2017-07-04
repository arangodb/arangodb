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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_BASE_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_BASE_HANDLER_H 1

#include "GeneralServer/RestHandler.h"

#include "Rest/GeneralResponse.h"

namespace arangodb {
namespace transaction {
class Context;
}

namespace velocypack {
class Builder;
struct Options;
class Slice;
}

class RestBaseHandler : public rest::RestHandler {
 public:
  explicit RestBaseHandler(GeneralRequest*, GeneralResponse*);

  void handleError(basics::Exception const&) override;

 public:
  // generates a result from VelocyPack
  template <typename Payload>
  void generateResult(rest::ResponseCode, Payload&&);

  // generates a result from VelocyPack
  template <typename Payload>
  void generateResult(rest::ResponseCode, Payload&&, VPackOptions const*);

  // generates a result from VelocyPack
  template <typename Payload>
  void generateResult(rest::ResponseCode, Payload&&,
                      std::shared_ptr<transaction::Context> context);
  
  /// convenience function akin to generateError,
  /// renders payload in 'result' field
  void generateSuccess(rest::ResponseCode, velocypack::Slice const&);

  // generates an error
  void generateError(rest::ResponseCode, int);

  // generates an error
  void generateError(rest::ResponseCode, int, std::string const&);
  
  // generates an error
  void generateError(arangodb::Result const&);

  // generates a canceled message
  void generateCanceled();

 protected:
  /// @brief parses the body as VelocyPack
  std::shared_ptr<arangodb::velocypack::Builder> parseVelocyPackBody(bool& success);

  template <typename Payload>
  void writeResult(Payload&&, arangodb::velocypack::Options const& options);
};
}

#endif
