////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include <fuerte/requests.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb { namespace fuerte { inline namespace v1 {
// Helper and Implementation
std::unique_ptr<Request> createRequest(RequestHeader&& messageHeader,
                                       StringMap&& headerStrings,
                                       RestVerb const& verb,
                                       ContentType const& contentType) {
  if (!headerStrings.empty()) {
    messageHeader.meta = std::move(headerStrings);
  }
  std::unique_ptr<Request> request(new Request(std::move(messageHeader)));

  request->header.restVerb = verb;
  request->header.contentType(contentType);
  request->header.acceptType(contentType);
  // fuerte requests default to vpack content type for accept
  //request->header.acceptType(ContentType::VPack);

  return request;
}

std::unique_ptr<Request> createRequest(RestVerb const& verb,
                                       ContentType const& contentType) {
  return createRequest(RequestHeader(), StringMap(), verb, contentType);
}

// For User
std::unique_ptr<Request> createRequest(RestVerb verb, std::string const& path,
                                       StringMap const& parameters,
                                       VPackBuffer<uint8_t>&& payload) {
  auto request = createRequest(verb, ContentType::VPack);
  request->header.path = path;
  request->header.parameters = parameters;
  request->addVPack(std::move(payload));
  return request;
}

std::unique_ptr<Request> createRequest(RestVerb verb, std::string const& path,
                                       StringMap const& parameters,
                                       VPackSlice const& payload) {
  auto request = createRequest(verb, ContentType::VPack);
  request->header.path = path;
  request->header.parameters = parameters;
  request->addVPack(payload);
  return request;
}

std::unique_ptr<Request> createRequest(RestVerb verb, std::string const& path,
                                       StringMap const& parameters) {
  auto request = createRequest(verb, ContentType::VPack);
  request->header.path = path;
  request->header.parameters = parameters;
  return request;
}
}}}  // namespace arangodb::fuerte::v1
