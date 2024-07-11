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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"

#include <string>
#include <string_view>

namespace arangodb {

// container for complete multipart message
struct MultipartMessage {
  MultipartMessage(std::string_view boundary, std::string_view message)
      : boundary(boundary), message(message) {}
  MultipartMessage() = default;

  bool skipBoundaryStart(size_t& offset) const;
  bool findBoundaryEnd(size_t& offset) const;
  bool isAtEnd(size_t& offset) const;
  std::string getHeader(size_t& offset) const;

  std::string_view boundary;
  std::string_view message;
};

// container for search data within multipart message
struct SearchHelper {
  MultipartMessage message;
  size_t searchStart = 0;
  std::string_view found;
  std::string contentId;
  bool containsMore = false;
};

class RestBatchHandler : public RestVocbaseBaseHandler {
 public:
  RestBatchHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);
  ~RestBatchHandler();

  RestStatus execute() override;
  char const* name() const override final { return "RestBatchHandler"; }
  // be pessimistic about what this handler does... it may invoke V8
  // or not, but as we don't know where, we need to assume it
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }

 private:
  // extract the boundary from the body of a multipart message
  bool getBoundaryBody(std::string&);

  // extract the boundary from the HTTP header of a multipart message
  bool getBoundaryHeader(std::string&);

  // extract the boundary of a multipart message
  bool getBoundary(std::string&);

  // extract the next part from a multipart message
  bool extractPart(SearchHelper&);

  bool executeNextHandler();
  void processSubHandlerResult(RestHandler const& handler);

  MultipartMessage _multipartMessage;
  SearchHelper _helper;
  size_t _errors;
  std::string _boundary;
};
}  // namespace arangodb
