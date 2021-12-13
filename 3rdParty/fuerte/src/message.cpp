////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include <fuerte/detail/vst.h>
#include <fuerte/helper.h>
#include <fuerte/message.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <sstream>

#include "debugging.h"

namespace {
static std::string const emptyString;

inline int hex2int(char ch, int errorCode) {
  if ('0' <= ch && ch <= '9') {
    return ch - '0';
  } else if ('A' <= ch && ch <= 'F') {
    return ch - 'A' + 10;
  } else if ('a' <= ch && ch <= 'f') {
    return ch - 'a' + 10;
  }

  return errorCode;
}
} // namespace

namespace arangodb { namespace fuerte { inline namespace v1 {

///////////////////////////////////////////////
// class MessageHeader
///////////////////////////////////////////////

void MessageHeader::setMeta(StringMap map) {
  if (!this->_meta.empty()) {
    for (auto& pair : map) {
      this->addMeta(std::move(pair.first), std::move(pair.second));
    }
  } else {
    this->_meta = std::move(map);
  }
}

// Get value for header metadata key, returns empty string if not found.
std::string const& MessageHeader::metaByKey(std::string const& key,
                                            bool& found) const {
  if (_meta.empty()) {
    found = false;
    return ::emptyString;
  }
  auto const& it = _meta.find(key);
  if (it == _meta.end()) {
    found = false;
    return ::emptyString;
  } else {
    found = true;
    return it->second;
  }
}

///////////////////////////////////////////////
// class RequestHeader
///////////////////////////////////////////////

void RequestHeader::acceptType(std::string const& type) {
  addMeta(fu_accept_key, type);
}

/// @brief analyze path and split into components
/// strips /_db/<name> prefix, sets db name and fills parameters
void RequestHeader::parseArangoPath(std::string_view p) {
  this->path = extractPathParameters(p, this->parameters);

  // extract database prefix /_db/<name>/
  const char* q = this->path.c_str();
  if (this->path.size() >= 4 && q[0] == '/' && q[1] == '_' && q[2] == 'd' &&
      q[3] == 'b' && q[4] == '/') {
    // request contains database name
    q += 5;
    const char* pathBegin = q;
    // read until end of database name
    while (*q != '\0' && *q != '/' && *q != '?' && *q != ' ' && *q != '\n' &&
           *q != '\r') {
      ++q;
    }
    FUERTE_ASSERT(q >= pathBegin);
    this->database.clear();
    char const* p = pathBegin;
    while (p != q) {
      std::string::value_type c = (*p);
      if (c == '%') {
        if (p + 2 < q) {
          int h = ::hex2int(p[1], 256) << 4;
          h += ::hex2int(p[2], 256);
          if (h >= 256) {
            throw std::invalid_argument("invalid encoding value in request URL");
          }
          this->database.push_back(static_cast<char>(h & 0xFF));
          p += 2;
        } else {
          throw std::invalid_argument("invalid encoding value in request URL");
        }
      } else if (c == '+') {
        this->database.push_back(' ');
      } else {
        this->database.push_back(c);
      }
      ++p;
    }
    if (*q == '\0') {
      this->path = "/";
    } else {
      this->path = std::string(q, this->path.c_str() + this->path.size());
    }
  }
}

///////////////////////////////////////////////
// class Message
///////////////////////////////////////////////

ContentEncoding Message::contentEncoding() const {
  return messageHeader().contentEncoding();
}

// content-type header accessors

ContentType Message::contentType() const {
  return messageHeader().contentType();
}

bool Message::isContentTypeJSON() const {
  return (contentType() == ContentType::Json);
}

bool Message::isContentTypeVPack() const {
  return (contentType() == ContentType::VPack);
}

bool Message::isContentTypeHtml() const {
  return (contentType() == ContentType::Html);
}

bool Message::isContentTypeText() const {
  return (contentType() == ContentType::Text);
}

///////////////////////////////////////////////
// class Request
///////////////////////////////////////////////

constexpr std::chrono::milliseconds Request::defaultTimeout;

ContentType Request::acceptType() const { return header.acceptType(); }

//// add payload add VelocyPackData
void Request::addVPack(VPackSlice const slice) {
#ifdef FUERTE_CHECKED_MODE
  // FUERTE_LOG_ERROR << "Checking data that is added to the message: " <<
  // std::endl;
  vst::parser::validateAndCount(slice.start(), slice.byteSize());
#endif

  header.contentType(ContentType::VPack);
  _payload.append(slice.start(), slice.byteSize());
}

void Request::addVPack(VPackBuffer<uint8_t> const& buffer) {
#ifdef FUERTE_CHECKED_MODE
  // FUERTE_LOG_ERROR << "Checking data that is added to the message: " <<
  // std::endl;
  vst::parser::validateAndCount(buffer.data(), buffer.byteSize());
#endif
  header.contentType(ContentType::VPack);
  _payload.append(buffer);
}

void Request::addVPack(VPackBuffer<uint8_t>&& buffer) {
#ifdef FUERTE_CHECKED_MODE
  // FUERTE_LOG_ERROR << "Checking data that is added to the message: " <<
  // std::endl;
  vst::parser::validateAndCount(buffer.data(), buffer.byteSize());
#endif
  header.contentType(ContentType::VPack);
  _payload = std::move(buffer);
}

// add binary data
void Request::addBinary(uint8_t const* data, std::size_t length) {
  _payload.append(data, length);
}

// get payload as slices
std::vector<VPackSlice> Request::slices() const {
  std::vector<VPackSlice> slices;
  if (isContentTypeVPack()) {
    auto length = _payload.byteSize();
    auto cursor = _payload.data();
    while (length) {
      slices.emplace_back(cursor);
      auto sliceSize = slices.back().byteSize();
      if (length < sliceSize) {
        throw std::logic_error("invalid buffer");
      }
      cursor += sliceSize;
      length -= sliceSize;
    }
  }
  return slices;
}

// get payload as binary
asio_ns::const_buffer Request::payload() const {
  return asio_ns::const_buffer(_payload.data(), _payload.byteSize());
}

size_t Request::payloadSize() const { return _payload.byteSize(); }

///////////////////////////////////////////////
// class Response
///////////////////////////////////////////////

std::vector<VPackSlice> Response::slices() const {
  std::vector<VPackSlice> slices;
  if (isContentTypeVPack() && payloadSize() > 0) {
    VPackValidator validator;

    auto length = _payload.byteSize() - _payloadOffset;
    auto cursor = _payload.data() + _payloadOffset;
    while (length) {
      // will throw on an error
      validator.validate(cursor, length, true);

      slices.emplace_back(cursor);
      auto sliceSize = slices.back().byteSize();
      if (length < sliceSize) {
        throw std::logic_error("invalid buffer");
      }
      FUERTE_ASSERT(length >= sliceSize);
      cursor += sliceSize;
      length -= sliceSize;
    }
  }
  return slices;
}

asio_ns::const_buffer Response::payload() const {
  return asio_ns::const_buffer(_payload.data() + _payloadOffset,
                               _payload.byteSize() - _payloadOffset);
}

size_t Response::payloadSize() const {
  auto payloadByteSize = _payload.byteSize();
  if (_payloadOffset > payloadByteSize) {
    return 0;
  }
  return payloadByteSize - _payloadOffset;
}

std::shared_ptr<velocypack::Buffer<uint8_t>> Response::copyPayload() const {
  auto buffer = std::make_shared<velocypack::Buffer<uint8_t>>();
  if (payloadSize() > 0) {
    buffer->append(_payload.data() + _payloadOffset,
                   _payload.byteSize() - _payloadOffset);
  }
  return buffer;
}

std::shared_ptr<velocypack::Buffer<uint8_t>> Response::stealPayload() {
  if (_payloadOffset == 0) {
    return std::make_shared<velocypack::Buffer<uint8_t>>(std::move(_payload));
  }

  auto buffer = std::make_shared<velocypack::Buffer<uint8_t>>();
  buffer->append(_payload.data() + _payloadOffset,
                 _payload.byteSize() - _payloadOffset);
  _payload.clear();
  _payloadOffset = 0;
  return buffer;
}
}}}  // namespace arangodb::fuerte::v1
