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
/// @author Julia Puget
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpResponseChecker.h"

#include "Basics/StaticStrings.h"

using namespace arangodb;

/// @brief Check for error in http response
Result HttpResponseChecker::check(std::string const& clientErrorMsg, httpclient::SimpleHttpResult const* const response,
                                  std::string const& actionMsg, std::string const& requestPayload) {
  using basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL,
            "got invalid response from server " + (clientErrorMsg.empty() ? "" : ": '" + clientErrorMsg + "'") +
                (actionMsg.empty() ? "" : " while executing " + actionMsg) +
                (requestPayload.empty() ? "" : " with this requestPayload: '" + requestPayload + "'")};
  }
  if (response->wasHttpError()) {
    auto errorNum = static_cast<int>(TRI_ERROR_INTERNAL);
    std::string errorMsg = response->getHttpReturnMessage();
    try {
      std::shared_ptr<velocypack::Builder> bodyBuilder(response->getBodyVelocyPack());
      velocypack::Slice error = bodyBuilder->slice();
      if (!error.isNone() && error.hasKey(StaticStrings::ErrorMessage)) {
        errorNum = error.get(StaticStrings::ErrorNum).getNumericValue<int>();
        errorMsg = error.get(StaticStrings::ErrorMessage).copyString();
      }
    } catch (...) {
      errorMsg = response->getHttpReturnMessage();
      errorNum = response->getHttpReturnCode();
    }

    auto err = ErrorCode{errorNum};
    return {err, "got invalid response from server: HTTP " +
                     itoa(response->getHttpReturnCode()) + ": '" + errorMsg + "'" +
                     (actionMsg.empty() ? "" : " while executing " + actionMsg) +
                     (requestPayload.empty() ? "" : " with this requestPayload: '" + requestPayload + "'")};
  }
  return {TRI_ERROR_NO_ERROR};
}