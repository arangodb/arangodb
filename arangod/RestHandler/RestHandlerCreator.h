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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Rest/GeneralRequest.h"
#include "SystemMonitor/Activities/RestHandler.h"
#include "Utils/ExecContext.h"

#include <memory>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace rest {
class RestHandler;
}
class GeneralResponse;

template<typename H>
class RestHandlerCreator : public H {
  template<typename... Args>
  static auto createAsSharedPtr(application_features::ApplicationServer& server,
                                GeneralRequest* request,
                                GeneralResponse* response, Args&&... args)
      -> std::shared_ptr<rest::RestHandler> {
    auto const deleter = [](H* h) {
      auto* request = h->request();
      // Call the destructor under the user's ExecContext, if any.
      auto guard = ExecContextScope(
          request != nullptr ? request->requestContext() : nullptr);
      delete h;
    };

    // Call the constructor under the user's ExecContext, if any.
    auto guard = ExecContextScope(request != nullptr ? request->requestContext()
                                                     : nullptr);

    // use the ExecContext-aware deleter
    return {new H(server, request, response, std::forward<Args>(args)...),
            deleter};
  }

 public:
  template<typename D>
  static std::shared_ptr<rest::RestHandler> createData(
      application_features::ApplicationServer& server, GeneralRequest* request,
      GeneralResponse* response, void* data) {
    auto h = createAsSharedPtr(server, request, response, (D)data);
    h->startActivity();
    return h;
  }

  static std::shared_ptr<rest::RestHandler> createNoData(
      application_features::ApplicationServer& server, GeneralRequest* request,
      GeneralResponse* response, void*) {
    auto h = createAsSharedPtr(server, request, response);
    h->startActivity();
    return h;
  }

  // TODO consolidate methods using variadic templates
};
}  // namespace arangodb
