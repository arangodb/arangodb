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

#include "RequestCoding.h"

#include "Basics/EncodingUtils.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ResultT.h"
#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"

#include <absl/strings/str_cat.h>
#include <fuerte/message.h>

#include <filesystem>

namespace fu = arangodb::fuerte;

namespace arangodb::request_coding {

ErrorCode fuerteToArangoErrorCode(fu::Error ec) {
  ErrorCode errorNumber = TRI_ERROR_NO_ERROR;
  switch (ec) {
    case fu::Error::CouldNotConnect:
    case fu::Error::CloseRequested:
    case fu::Error::ConnectionClosed:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT;
      break;

    case fu::Error::ReadError:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ;
      break;

    case fu::Error::WriteError:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE;
      break;

    case fu::Error::RequestTimeout:
      errorNumber = TRI_ERROR_HTTP_REQUEST_TIMEOUT;
      break;

    case fu::Error::QueueCapacityExceeded:
    case fu::Error::ConnectionCanceled:
    case fu::Error::ProtocolError:
    default:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR;
      break;
  }
  return errorNumber;
}

void translateHeaders(
    fu::Request& request, fu::RestVerb const method, std::string_view location,
    std::string const& databaseName, bool forceJson,
    std::chrono::milliseconds timeout,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool requestCompression) {
  request.header.restVerb = method;
  request.header.database = databaseName;
  request.header.parseArangoPath(location);
  if (forceJson) {
    // Preset posting json (if) but allow override if there is a specified
    // header:
    request.header.contentType(fu::ContentType::Json);
    request.header.acceptType(fu::ContentType::Json);
  }
  for (auto const& pair : headerFields) {
    request.header.addMeta(basics::StringUtils::tolower(pair.first),
                           pair.second);
  }
  if (request.header.acceptType() == fu::ContentType::Unset) {
    request.header.acceptType(fu::ContentType::VPack);
  }
  if (requestCompression &&
      !request.header.meta().contains(StaticStrings::AcceptEncoding)) {
    request.header.addMeta(StaticStrings::AcceptEncoding,
                           StaticStrings::EncodingDeflate);
  }

  request.timeout(timeout);
}

Result addRequestBody(fu::Request& request, RequestBody const& body,
                      bool forceJson, uint64_t compressRequestThreshold) {
  // yields whether data went into the request deflated
  auto compressIfEligible = [&](std::string_view data) -> ResultT<bool> {
    if (compressRequestThreshold == 0) {
      // opted out of compression
      return false;
    }
    if (request.header.meta().contains(StaticStrings::ContentEncoding)) {
      // we don't compress if there is already a Content-Encoding header
      return false;
    }
    if (data.size() < compressRequestThreshold) {
      return false;
    }
    auto& pfm = request.payloadForModification();
    TRI_ASSERT(pfm.empty());
    if (encoding::zlibDeflate(reinterpret_cast<uint8_t const*>(data.data()),
                              data.size(), pfm) != TRI_ERROR_NO_ERROR) {
      return ResultT<bool>::error(TRI_ERROR_INTERNAL,
                                  "unable to compress request body");
    }
    if (pfm.size() >= data.size()) {
      // size after compression is higher than before compression...
      // clear again and fall back to regular, uncompressed encoding.
      pfm.clear();
      return false;
    }
    request.header.addMeta(StaticStrings::ContentEncoding,
                           StaticStrings::EncodingDeflate);
    return true;
  };
  auto addBinary = [&](std::string_view data) -> Result {
    auto compressed = compressIfEligible(data);
    if (compressed.fail()) {
      return compressed.result();
    }
    if (!compressed.get()) {
      request.addBinary(reinterpret_cast<uint8_t const*>(data.data()),
                        data.size());
    }
    return {};
  };

  if (auto const* file = std::get_if<FileBody>(&body)) {
    std::error_code existsEc;
    if (!std::filesystem::exists(file->path, existsEc)) {
      return {
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("file to load for body doesn't exist: ", file->path)};
    }
    std::string contents;
    try {
      contents = basics::FileUtils::slurp(file->path);
    } catch (...) {
      std::string err = absl::StrCat("could not read file", file->path);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_errno(), err);
    }
    request.header.contentType(fu::ContentType::Custom);
    request.addBinary(reinterpret_cast<uint8_t const*>(contents.data()),
                      contents.length());
  } else if (auto const* json = std::get_if<JsonBody>(&body)) {
    if (auto res = addBinary(json->text); res.fail()) {
      return res;
    }
    if (request.header.contentType() == fu::ContentType::Unset) {
      request.header.contentType(fu::ContentType::Json);
    }
  } else if (auto const* binary = std::get_if<BinaryBody>(&body)) {
    if (auto res = addBinary(binary->data); res.fail()) {
      return res;
    }
  } else if (auto const* value = std::get_if<ValueBody>(&body)) {
    if (forceJson) {
      if (auto res = addBinary(value->value.toJson()); res.fail()) {
        return res;
      }
      request.header.contentType(fu::ContentType::Json);
    } else {
      auto compressed = compressIfEligible(
          std::string_view(reinterpret_cast<char const*>(value->value.start()),
                           value->value.byteSize()));
      if (compressed.fail()) {
        return compressed.result();
      }
      if (!compressed.get()) {
        request.addVPack(value->value);
      }
      request.header.contentType(fu::ContentType::VPack);
    }
  } else if (request.header.contentType() == fu::ContentType::Unset) {
    request.header.contentType(fu::ContentType::Json);
  }
  return {};
}

ErrorCode uncompressResponse(fu::Response& response) {
  ErrorCode code = TRI_ERROR_NO_ERROR;
  if (response.contentEncoding() == fuerte::ContentEncoding::Deflate ||
      response.contentEncoding() == fuerte::ContentEncoding::Gzip) {
    // transparently handling deflate/gzip encoded responses
    auto responseBody = response.payload();
    VPackBuffer<uint8_t> inflateBuf;
    if (response.contentEncoding() == fuerte::ContentEncoding::Deflate) {
      code = arangodb::encoding::zlibInflate(
          reinterpret_cast<uint8_t const*>(responseBody.data()),
          responseBody.size(), inflateBuf);
    } else {
      TRI_ASSERT(response.contentEncoding() == fuerte::ContentEncoding::Gzip);
      code = arangodb::encoding::gzipUncompress(
          reinterpret_cast<uint8_t const*>(responseBody.data()),
          responseBody.size(), inflateBuf);
    }
    if (code != TRI_ERROR_NO_ERROR) {
      return code;
    }
    // replace response body with uncompressed value
    response.setPayload(std::move(inflateBuf), 0);
    // reset Content-Encoding, so that we do not accidentially
    // uncompress again somewhere downstream
    response.header.contentEncoding(fu::ContentEncoding::Identity);
    response.header.removeMeta(StaticStrings::ContentEncoding);
  }
  return code;
}

}  // namespace arangodb::request_coding
