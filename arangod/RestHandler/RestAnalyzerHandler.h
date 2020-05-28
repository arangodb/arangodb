////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_ANALYZER_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_ANALYZER_HANDLER_H 1

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "utils/string.hpp"

namespace arangodb {
namespace iresearch {

class IResearchAnalyzerFeature; // forward declaration

class RestAnalyzerHandler: public RestVocbaseBaseHandler {
 public:
  // @note RestHandlerFactory::createHandler(...) passes raw pointers for
  //       request/response to RestHandlerCreator::createNoData(...)
  RestAnalyzerHandler(  // constructor
      application_features::ApplicationServer& server,
      arangodb::GeneralRequest* request,   // request
      arangodb::GeneralResponse* response  // response
  );

  virtual arangodb::RestStatus execute() override;

  virtual arangodb::RequestLane lane() const override {
    return arangodb::RequestLane::CLIENT_SLOW;
  }

  virtual char const* name() const override { return "RestAnalyzerHandler"; }

 private:
  void createAnalyzer(IResearchAnalyzerFeature& analyzers);
  void getAnalyzer(
    IResearchAnalyzerFeature& analyzers, 
    std::string const& requestedName 
  );
  void getAnalyzers(IResearchAnalyzerFeature& analyzers);
  void removeAnalyzer(
    IResearchAnalyzerFeature& analyzers, 
    std::string const& requestedName, 
    bool force
  );
};

} // iresearch
} // arangodb

#endif
