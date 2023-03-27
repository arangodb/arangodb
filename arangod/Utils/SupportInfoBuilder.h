////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/arangod.h"

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class SupportInfoBuilder {
 public:
  SupportInfoBuilder() = delete;
  static void buildInfoMessage(velocypack::Builder& result,
                               std::string const& dbName, ArangodServer& server,
                               bool isLocal, bool isTemeletricsReq = false);
  static void buildDbServerDataStoredInfo(velocypack::Builder& result,
                                          ArangodServer& server);

 private:
  static void addDatabaseInfo(velocypack::Builder& result,
                              velocypack::Slice infoSlice,
                              ArangodServer& server);
  static void buildHostInfo(velocypack::Builder& result, ArangodServer& server,
                            bool isTelemetricsReq);
  static void normalizeKeyForTelemetrics(std::string& key);
};

}  // namespace arangodb
