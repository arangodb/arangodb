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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_BATCH_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_BATCH_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

// container for complete multipart message
struct MultipartMessage {
  MultipartMessage(char const* boundary, size_t const boundaryLength,
                   char const* messageStart, char const* messageEnd)
      : boundary(boundary),
        boundaryLength(boundaryLength),
        messageStart(messageStart),
        messageEnd(messageEnd){};

  char const* boundary;
  size_t const boundaryLength;
  char const* messageStart;
  char const* messageEnd;
};

// container for search data within multipart message
struct SearchHelper {
  MultipartMessage* message;
  char const* searchStart;
  char const* foundStart;
  size_t foundLength;
  char const* contentId;
  size_t contentIdLength;
  bool containsMore;
};

class RestBatchHandler : public RestVocbaseBaseHandler {
 public:
  RestBatchHandler(GeneralRequest*, GeneralResponse*);
  ~RestBatchHandler();

 public:
  RestHandler::status execute() override;

 private:
  RestHandler::status executeHttp();
  RestHandler::status executeVpp();
  // extract the boundary from the body of a multipart message
  bool getBoundaryBody(std::string*);

  // extract the boundary from the HTTP header of a multipart message
  bool getBoundaryHeader(std::string*);

  // extract the boundary of a multipart message
  bool getBoundary(std::string*);

  // extract the next part from a multipart message
  bool extractPart(SearchHelper*);
};
}

#endif
