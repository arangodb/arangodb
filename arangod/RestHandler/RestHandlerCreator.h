////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_HANDLER_REST_HANDLER_CREATOR_H
#define ARANGOD_REST_HANDLER_REST_HANDLER_CREATOR_H 1

#include "Basics/Common.h"

#include <memory>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace rest {
class RestHandler;
}

template <typename H>
class RestHandlerCreator : public H {
 public:
  template <typename D>
  static std::shared_ptr<rest::RestHandler> createData(application_features::ApplicationServer& server,
                                                       GeneralRequest* request,
                                                       GeneralResponse* response, void* data) {
    return std::make_shared<H>(server, request, response, (D)data);
  }

  static std::shared_ptr<rest::RestHandler> createNoData(application_features::ApplicationServer& server,
                                                         GeneralRequest* request,
                                                         GeneralResponse* response, void*) {
    return std::make_shared<H>(server, request, response);
  }

  // TODO consolidate methods using variadic templates
};
}  // namespace arangodb

#endif
