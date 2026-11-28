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

#include "Basics/ErrorCode.h"
#include "Basics/Result.h"

#include <fuerte/types.h>
#include <velocypack/Slice.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace arangodb {
namespace fuerte {
inline namespace v1 {
class Request;
class Response;
}  // namespace v1
}  // namespace fuerte

// How the client turns its calls into fuerte requests, and their responses
// back into payloads it can read
namespace request_coding {

struct NoBody {};
struct JsonBody {
  std::string_view text;
};
struct BinaryBody {
  std::string_view data;
};
// sent as VelocyPack, or as JSON if JSON is forced
struct ValueBody {
  velocypack::Slice value;
};
// never compressed
struct FileBody {
  std::string path;
};
using RequestBody =
    std::variant<NoBody, JsonBody, BinaryBody, ValueBody, FileBody>;

// headerFields override what forceJson presets; requestCompression asks the
// server for a deflated response
void translateHeaders(
    fuerte::Request& request, fuerte::RestVerb method,
    std::string_view location, std::string const& databaseName, bool forceJson,
    std::chrono::milliseconds timeout,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool requestCompression);

// Puts body into request, deflated if compressRequestThreshold allows it and
// it pays off. 0 disables compression
Result addRequestBody(fuerte::Request& request, RequestBody const& body,
                      bool forceJson, uint64_t compressRequestThreshold);

// Inflates a deflate or gzip encoded payload in place
ErrorCode uncompressResponse(fuerte::Response& response);

ErrorCode fuerteToArangoErrorCode(fuerte::Error error);

}  // namespace request_coding
}  // namespace arangodb
