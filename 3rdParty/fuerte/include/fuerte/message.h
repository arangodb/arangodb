////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Ewout Prangsma
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_MESSAGE
#define ARANGO_CXX_DRIVER_MESSAGE

#include <fuerte/asio_ns.h>
#include <fuerte/types.h>
#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb {
namespace fuerte {
inline namespace v1 {
const std::string fu_accept_key("accept");
const std::string fu_authorization_key("authorization");
const std::string fu_content_length_key("content-length");
const std::string fu_content_type_key("content-type");
const std::string fu_content_encoding_key("content-encoding");
const std::string fu_keep_alive_key("keep-alive");

struct MessageHeader {
  // Header metadata helpers#
  template<typename K, typename V>
  void addMeta(K&& key, V&& value) {
    if (fu_accept_key == key) {
      _acceptType = to_ContentType(value);
      if (_acceptType != ContentType::Custom) {
        return;
      }
    } else if (fu_content_type_key == key) {
      _contentType = to_ContentType(value);
      if (_contentType != ContentType::Custom) {
        return;
      }
    } else if (fu_content_encoding_key == key) {
      _contentEncoding = to_ContentEncoding(value);
    }
    this->_meta.emplace(std::forward<K>(key), std::forward<V>(value));
  }

  void setMeta(StringMap);
  StringMap const& meta() const { return _meta; }

  // Get value for header metadata key, returns empty string if not found.
  std::string const& metaByKey(std::string const& key) const {
    bool unused;
    return this->metaByKey(key, unused);
  }
  std::string const& metaByKey(std::string const& key, bool& found) const;

  void removeMeta(std::string const& key) { _meta.erase(key); }

  ContentEncoding contentEncoding() const { return _contentEncoding; }
  void contentEncoding(ContentEncoding encoding) noexcept {
    _contentEncoding = encoding;
  }

  // content type accessors
  ContentType contentType() const { return _contentType; }
  void contentType(ContentType type) { _contentType = type; }
  void contentType(std::string const& type) {
    addMeta(fu_content_type_key, type);
  }

 protected:
  StringMap _meta;     /// Header meta data (equivalent to HTTP headers)
  ContentType _contentType = ContentType::Unset;
  ContentType _acceptType = ContentType::VPack;
  ContentEncoding _contentEncoding = ContentEncoding::Identity;
};

struct RequestHeader final : public MessageHeader {
  /// Database that is the target of the request
  std::string database;

  /// Local path of the request (without "/_db/" prefix)
  std::string path;

  /// Query parameters
  StringMap parameters;

  /// HTTP method
  RestVerb restVerb = RestVerb::Illegal;

  // accept header accessors
  ContentType acceptType() const { return _acceptType; }
  void acceptType(ContentType type) { _acceptType = type; }
  void acceptType(std::string const& type);

  /// @brief analyze path and split into components
  /// strips /_db/<name> prefix, sets db name and fills parameters
  void parseArangoPath(std::string_view path);
};

struct ResponseHeader final : public MessageHeader {
  friend class Response;

  /// Response code
  StatusCode responseCode = StatusUndefined;

  MessageType responseType() const { return MessageType::Response; }
};

// Message is base class for message being send to (Request) or
// from (Response) a server.
class Message {
 protected:
  Message() : _timestamp(std::chrono::steady_clock::now()) {}
  virtual ~Message() = default;

 public:
  /// Message type
  virtual MessageType type() const = 0;
  virtual MessageHeader const& messageHeader() const = 0;

  ///////////////////////////////////////////////
  // get payload
  ///////////////////////////////////////////////

  /// get slices if the content-type is velocypack
  virtual std::vector<velocypack::Slice> slices() const = 0;
  virtual asio_ns::const_buffer payload() const = 0;
  virtual std::size_t payloadSize() const = 0;
  std::string payloadAsString() const {
    auto p = payload();
    return std::string(asio_ns::buffer_cast<char const*>(p),
                       asio_ns::buffer_size(p));
  }

  /// get the content as a slice
  velocypack::Slice slice() const {
    auto slices = this->slices();
    if (!slices.empty()) {
      return slices[0];
    }
    return velocypack::Slice::noneSlice();
  }

  /// content-encoding header type
  ContentEncoding contentEncoding() const;

  /// content-type header accessors
  ContentType contentType() const;

  bool isContentTypeJSON() const;
  bool isContentTypeVPack() const;
  bool isContentTypeHtml() const;
  bool isContentTypeText() const;

  std::chrono::steady_clock::time_point timestamp() const { return _timestamp; }
  // set timestamp when it was sent
  void timestamp(std::chrono::steady_clock::time_point t) { _timestamp = t; }

 private:
  std::chrono::steady_clock::time_point _timestamp;
};

// Request contains the message send to a server in a request.
class Request final : public Message {
 public:
  static constexpr std::chrono::milliseconds defaultTimeout =
      std::chrono::seconds(300);

  Request(RequestHeader messageHeader = RequestHeader())
      : header(std::move(messageHeader)), _timeout(defaultTimeout) {}

  /// @brief request header
  RequestHeader header;

  MessageType type() const override { return MessageType::Request; }
  MessageHeader const& messageHeader() const override { return header; }
  void setFuzzReqHeader(std::string fuzzHeader) {
    _fuzzReqHeader = std::move(fuzzHeader);
  }
  std::optional<std::string> getFuzzReqHeader() const { return _fuzzReqHeader; }
  bool getFuzzerReq() const noexcept { return _fuzzReqHeader.has_value(); }

  ///////////////////////////////////////////////
  // header accessors
  ///////////////////////////////////////////////

  // accept header accessors
  ContentType acceptType() const;

  ///////////////////////////////////////////////
  // add payload
  ///////////////////////////////////////////////
  void addVPack(velocypack::Slice const slice);
  void addVPack(velocypack::Buffer<uint8_t> const& buffer);
  void addVPack(velocypack::Buffer<uint8_t>&& buffer);
  void addBinary(uint8_t const* data, std::size_t length);

  ///////////////////////////////////////////////
  // get payload
  ///////////////////////////////////////////////

  /// @brief get velocypack slices contained in request
  /// only valid iff the data was added via addVPack
  std::vector<velocypack::Slice> slices() const override;
  velocypack::Buffer<uint8_t>& payloadForModification() { return _payload; }
  asio_ns::const_buffer payload() const override;
  std::size_t payloadSize() const override;
  velocypack::Buffer<uint8_t>&& moveBuffer() && { return std::move(_payload); }

  // get timeout, 0 means no timeout
  inline std::chrono::milliseconds timeout() const { return _timeout; }
  // set timeout
  void timeout(std::chrono::milliseconds timeout) { _timeout = timeout; }

  // Sending time accounting:
  void setTimeQueued() noexcept {
    _timeQueued = std::chrono::steady_clock::now();
  }
  void setTimeAsyncWrite() noexcept {
    _timeAsyncWrite = std::chrono::steady_clock::now();
  }
  void setTimeSent() noexcept { _timeSent = std::chrono::steady_clock::now(); }
  std::chrono::steady_clock::time_point timeQueued() const noexcept {
    return _timeQueued;
  }
  std::chrono::steady_clock::time_point timeAsyncWrite() const noexcept {
    return _timeAsyncWrite;
  }
  std::chrono::steady_clock::time_point timeSent() const noexcept {
    return _timeSent;
  }

 private:
  velocypack::Buffer<uint8_t> _payload;
  std::chrono::milliseconds _timeout;
  std::optional<std::string> _fuzzReqHeader = std::nullopt;
  std::chrono::steady_clock::time_point _timeQueued;
  std::chrono::steady_clock::time_point _timeAsyncWrite;
  std::chrono::steady_clock::time_point _timeSent;
};

// Response contains the message resulting from a request to a server.
class Response : public Message {
 public:
  Response(ResponseHeader reqHeader = ResponseHeader())
      : header(std::move(reqHeader)), _payloadOffset(0) {}

  Response(Response const&) = delete;
  Response& operator=(Response const&) = delete;

  /// @brief request header
  ResponseHeader header;

  MessageType type() const override { return header.responseType(); }
  MessageHeader const& messageHeader() const override { return header; }
  ///////////////////////////////////////////////
  // get / check status
  ///////////////////////////////////////////////

  // statusCode returns the (HTTP) status code for the request (200==OK).
  StatusCode statusCode() const noexcept { return header.responseCode; }

  // checkStatus returns true if the statusCode equals one of the given valid
  // code, false otherwise.
  bool checkStatus(std::initializer_list<StatusCode> validStatusCodes) const {
    auto actual = statusCode();
    for (auto code : validStatusCodes) {
      if (code == actual) {
        return true;
      }
    }
    return false;
  }
  // assertStatus throw an exception if the statusCode does not equal one of the
  // given valid codes.
  void assertStatus(std::initializer_list<StatusCode> validStatusCodes) {
    if (!checkStatus(validStatusCodes)) {
      throw std::runtime_error("invalid status " +
                               std::to_string(statusCode()));
    }
  }

  ///////////////////////////////////////////////
  // get/set payload
  ///////////////////////////////////////////////

  /// @brief validates and returns VPack response. Only valid for velocypack
  std::vector<velocypack::Slice> slices() const override;
  asio_ns::const_buffer payload() const override;
  std::size_t payloadSize() const override;
  std::shared_ptr<velocypack::Buffer<uint8_t>> copyPayload() const;
  std::shared_ptr<velocypack::Buffer<uint8_t>> stealPayload();

  /// @brief move in the payload
  void setPayload(velocypack::Buffer<uint8_t>&& buffer, std::size_t offset) {
    _payloadOffset = offset;
    _payload = std::move(buffer);
  }

 private:
  velocypack::Buffer<uint8_t> _payload;
  std::size_t _payloadOffset;
};
}  // namespace v1
}  // namespace fuerte
}  // namespace arangodb
#endif
