////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "utils/string.hpp"

namespace arangodb {
namespace iresearch {

class IResearchAnalyzerFeature;

class RestAnalyzerHandler : public RestVocbaseBaseHandler {
 public:
  RestAnalyzerHandler(ArangodServer& server, GeneralRequest* request,
                      GeneralResponse* response);

  virtual RestStatus execute() override;

  virtual RequestLane lane() const override { return RequestLane::CLIENT_SLOW; }

  virtual char const* name() const override { return "RestAnalyzerHandler"; }

 private:
  void createAnalyzer(IResearchAnalyzerFeature& analyzers);
  void getAnalyzer(IResearchAnalyzerFeature& analyzers,
                   std::string const& requestedName);
  void getAnalyzers(IResearchAnalyzerFeature& analyzers);
  void removeAnalyzer(IResearchAnalyzerFeature& analyzers,
                      std::string const& requestedName, bool force);
};

}  // namespace iresearch
}  // namespace arangodb
