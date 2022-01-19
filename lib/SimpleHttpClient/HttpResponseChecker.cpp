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
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpResponseChecker.h"

#include "Basics/StaticStrings.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief Trim the payload so it doesn't have the password field with the
/// password in plain text
void HttpResponseChecker::trimPayload(VPackSlice input, VPackBuilder& output) {
  using namespace velocypack;
  if (input.isObject()) {
    output.openObject();
    ObjectIterator it(input);
    while (it.valid()) {
      if (it.key().stringView() != "passwd") {
        output.add(it.key());
        trimPayload(it.value(), output);
      }
      it.next();
    }
    output.close();
  } else if (input.isArray()) {
    output.openArray();
    VPackArrayIterator it(input);
    while (it.valid()) {
      trimPayload(it.value(), output);
      it.next();
    }
    output.close();
  } else {
    output.add(input);
  }
}

/// @brief Check for error in http response
Result HttpResponseChecker::check(
    std::string const& clientErrorMsg,
    httpclient::SimpleHttpResult const* const response,
    std::string const& actionMsg, std::string_view requestPayload,
    PayloadType type) {
  if (response != nullptr && !response->wasHttpError() &&
      response->isComplete()) {
    return {TRI_ERROR_NO_ERROR};
  }
  // here we will trim the payload to remove every field that contains the
  // password in plain text, e.g. "passwd" field, and we don't have a way to
  // trim it with velocypack, so we have to parse the JSON and rebuild it
  // without the "passwd" fields.
  std::string msgBody;
  if (!requestPayload.empty()) {
    VPackBuilder output;
    if (type == PayloadType::JSON) {
      try {
        auto payloadVPack =
            VPackParser::fromJson(requestPayload.data(), requestPayload.size());
        trimPayload(payloadVPack->slice(), output);
        msgBody = output.toJson();
      } catch (...) {
        msgBody = requestPayload;
      }
    } else if (type == PayloadType::VPACK) {
      trimPayload(
          VPackSlice(reinterpret_cast<uint8_t const*>(requestPayload.data())),
          output);
      msgBody = output.toJson();
    } else {
      msgBody = requestPayload;
    }
  }
  constexpr size_t maxMsgBodySize = 4096;  // max amount of bytes in msg body to
                                           // truncate if too big to display
  if (msgBody.size() > maxMsgBodySize) {   // truncate error message
    msgBody.resize(maxMsgBodySize);
    msgBody.append("...");
  }
  using basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {
        TRI_ERROR_INTERNAL,
        "got invalid response from server " +
            (clientErrorMsg.empty() ? "" : ": '" + clientErrorMsg + "'") +
            (actionMsg.empty() ? "" : " while executing " + actionMsg) +
            (msgBody.empty() ? ""
                             : " with this requestPayload: '" + msgBody + "'")};
  }
  auto errorNum = static_cast<int>(TRI_ERROR_INTERNAL);
  std::string errorMsg = response->getHttpReturnMessage();
  try {
    std::shared_ptr<velocypack::Builder> bodyBuilder(
        response->getBodyVelocyPack());
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
  return {
      err,
      "got invalid response from server: HTTP " +
          itoa(response->getHttpReturnCode()) + ": '" + errorMsg + "'" +
          (actionMsg.empty() ? "" : " while executing " + actionMsg) +
          (msgBody.empty() ? ""
                           : " with this requestPayload: '" + msgBody + "'")};
}

/// @brief Check for error in http response
Result HttpResponseChecker::check(
    std::string const& clientErrorMsg,
    httpclient::SimpleHttpResult const* const response) {
  return check(clientErrorMsg, response, "", "", PayloadType::JSON);
}