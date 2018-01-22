
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_HANDLER_REST_STATISTICS_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_STATISTICS_HANDLER_H 1

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {
class RestStatisticsHandler : public arangodb::RestVocbaseBaseHandler {

  static uint32_t const statistics_interval = 10;
  static uint32_t const statistics_history_interval = 15 * 60;

 public:
  RestStatisticsHandler(GeneralRequest*, GeneralResponse*);

 public:
  char const* name() const override final { return "RestStatisticsHandler"; }
  bool isDirect() const override { return true; }
  RestStatus execute() override;
};
}

#endif
