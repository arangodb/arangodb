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

#include <string>
#include <vector>

#include <fuerte/asio_ns.h>
#include <fuerte/types.h>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb { namespace fuerte { inline namespace v1 {
const std::string fu_content_length_key("content-length");
const std::string fu_content_type_key("content-type");
const std::string fu_accept_key("accept");
  
struct MessageHeader {
  /// arangodb message format version
  short version() const { return _version; }
  void setVersion(short v) { _version = v; }
  
  /// Header meta data (equivalent to HTTP headers)
  StringMap meta;
  
#ifdef FUERTE_DEBUG
  std::size_t byteSize;    // for debugging
#endif
  
public:
  
  // Header metadata helpers
  void addMeta(std::string const& key, std::string const& value);
  void addMeta(StringMap const&);
  
  // Get value for header metadata key, returns empty string if not found.
  std::string const& metaByKey(std::string const& key) const;
  
  // content type accessors
  inline std::string const& contentTypeString() const {
    return metaByKey(fu_content_type_key);
  }
  
  inline ContentType contentType() const {
    return to_ContentType(contentTypeString());
  }
  
  void contentType(std::string const& type);
  void contentType(ContentType type);
  
protected:
  short _version;
};
  
struct RequestHeader final : public MessageHeader {
  
  /// HTTP method
  RestVerb restVerb = RestVerb::Illegal;
  
  /// Database that is the target of the request
  std::string database;
  
  /// Local path of the request (without "/_db/" prefix)
  std::string path;
  
  /// Query parameters
  StringMap parameters;
  
public:
  
  // accept header accessors
  std::string acceptTypeString() const;
  ContentType acceptType() const;
  void acceptType(std::string const& type);
  void acceptType(ContentType type);
  
  // query parameter helpers
  void addParameter(std::string const& key, std::string const& value);
  
  /// @brief analyze path and split into components
  /// strips /_db/<name> prefix, sets db name and fills parameters
  void parseArangoPath(std::string const&);
};
  
struct ResponseHeader final : public MessageHeader {
  friend class Response;

  /// Response code
  StatusCode responseCode = StatusUndefined;
  
  MessageType responseType() const { return _responseType; }
  
private:
  MessageType _responseType = MessageType::Response;
};

// Message is base class for message being send to (Request) or
// from (Response) a server.
class Message {
 protected:
  Message() = default;
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
  velocypack::Slice slice() {
    auto slices = this->slices();
    if (!slices.empty()) {
      return slices[0];
    }
    return velocypack::Slice::noneSlice();
  }

  /// content-type header accessors
  std::string contentTypeString() const;
  ContentType contentType() const;
  
  bool isContentTypeJSON() const;
  bool isContentTypeVPack() const;
  bool isContentTypeHtml() const;
  bool isContentTypeText() const;
};

// Request contains the message send to a server in a request.
class Request final : public Message {
 public:
  static constexpr std::chrono::milliseconds defaultTimeout = std::chrono::milliseconds(30 * 1000);

  Request(RequestHeader&& messageHeader = RequestHeader())
      : header(std::move(messageHeader)),
        _timeout(defaultTimeout) {}
  
  Request(RequestHeader const& messageHeader)
      : header(messageHeader),
        _timeout(defaultTimeout) {}
  
  /// @brief request header
  RequestHeader header;
  
  MessageType type() const override { return MessageType::Request; }
  MessageHeader const& messageHeader() const override { return header; }
  
  ///////////////////////////////////////////////
  // header accessors
  ///////////////////////////////////////////////
  
  // accept header accessors
  std::string acceptTypeString() const;
  ContentType acceptType() const;

  ///////////////////////////////////////////////
  // add payload
  ///////////////////////////////////////////////
  void addVPack(velocypack::Slice const& slice);
  void addVPack(velocypack::Buffer<uint8_t> const& buffer);
  void addVPack(velocypack::Buffer<uint8_t>&& buffer);
  void addBinary(uint8_t const* data, std::size_t length);

  ///////////////////////////////////////////////
  // get payload
  ///////////////////////////////////////////////
  
  /// @brief get velocypack slices contained in request
  /// only valid iff the data was added via addVPack
  std::vector<velocypack::Slice> slices() const override;
  asio_ns::const_buffer payload() const override;
  std::size_t payloadSize() const override;

  // get timeout, 0 means no timeout
  inline std::chrono::milliseconds timeout() const { return _timeout; }
  // set timeout
  void timeout(std::chrono::milliseconds timeout) { _timeout = timeout; }

 private:
  velocypack::Buffer<uint8_t> _payload;
  std::chrono::milliseconds _timeout;
};

// Response contains the message resulting from a request to a server.
class Response final : public Message {
 public:
  Response(ResponseHeader&& reqHeader = ResponseHeader())
      : header(std::move(reqHeader)), _payloadOffset(0) {}
  
  /// @brief request header
  ResponseHeader header;
  
  MessageType type() const override { return header._responseType; }
  MessageHeader const& messageHeader() const override { return header; }
  ///////////////////////////////////////////////
  // get / check status
  ///////////////////////////////////////////////

  // statusCode returns the (HTTP) status code for the request (200==OK).
  StatusCode statusCode() { return header.responseCode; }
  // checkStatus returns true if the statusCode equals one of the given valid
  // code, false otherwise.
  bool checkStatus(std::initializer_list<StatusCode> validStatusCodes) {
    auto actual = statusCode();
    for (auto code : validStatusCodes) {
      if (code == actual) return true;
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
  
  /// @brief move in the payload
  void setPayload(velocypack::Buffer<uint8_t> buffer, std::size_t payloadOffset);
    
 private:
  velocypack::Buffer<uint8_t> _payload;
  std::size_t _payloadOffset;
};
}}}  // namespace arangodb::fuerte::v1
#endif
