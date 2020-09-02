////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef APPLICATION_FEATURES_CONSOLE_FEATURE_H
#define APPLICATION_FEATURES_CONSOLE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "GeneralServer/OperationMode.h"
#include "RestServer/ConsoleThread.h"

namespace arangodb {

class ConsoleFeature final : public application_features::ApplicationFeature {
 public:
  explicit ConsoleFeature(application_features::ApplicationServer& server);

  void start() override final;
  void unprepare() override final;

 private:
  OperationMode _operationMode;
  std::unique_ptr<ConsoleThread> _consoleThread;
};

}  // namespace arangodb

#endif