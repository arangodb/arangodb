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

#include <fuerte/message.h>
#include <fuerte/detail/vst.h>


#include <sstream>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>


namespace arangodb { namespace fuerte { inline namespace v1 {
  
///////////////////////////////////////////////
// class MessageHeader
///////////////////////////////////////////////

void MessageHeader::addMeta(std::string const& key, std::string const& value) {
  meta.emplace(key, value);
}
  
void MessageHeader::addMeta(StringMap const& map) {
  for(auto& pair : map) {
    meta.insert(pair);
  }
}

// Get value for header metadata key, returns empty string if not found.
std::string const& MessageHeader::metaByKey(std::string const& key) const {
  static std::string emptyString("");
  if (meta.empty()) {
    return emptyString;
  }
  auto const& found = meta.find(key);
  if (found == meta.end()) {
    return emptyString;
  } else {
    return found->second;
  }
}
  

void MessageHeader::contentType(std::string const& type) {
  addMeta(fu_content_type_key, type);
}

void MessageHeader::contentType(ContentType type) {
  contentType(to_string(type));
}
  
///////////////////////////////////////////////
// class RequestHeader
///////////////////////////////////////////////
  
// accept header accessors
std::string RequestHeader::acceptTypeString() const {
  return metaByKey(fu_accept_key);
}

ContentType RequestHeader::acceptType() const {
  return to_ContentType(acceptTypeString());
}

void RequestHeader::acceptType(std::string const& type) {
  addMeta(fu_accept_key, type);
}

void RequestHeader::acceptType(ContentType type) {
  acceptType(to_string(type));
}
  
void RequestHeader::addParameter(std::string const& key,
                                 std::string const& value) {
  parameters.emplace(key, value);
}
  
/// @brief analyze path and split into components
/// strips /_db/<name> prefix, sets db name and fills parameters
void RequestHeader::parseArangoPath(std::string const& p) {
  size_t pos = p.rfind('?');
  if (pos != std::string::npos) {
    this->path = p.substr(0, pos);
    
    while (pos != std::string::npos && pos + 1 < p.length()) {
      size_t pos2 = p.find('=', pos + 1);
      if (pos2 == std::string::npos) {
        break;
      }
      std::string key = p.substr(pos + 1, pos2 - pos -1);
      pos = p.find('&', pos2 + 1); // points to next '&' or string::npos
      std::string value = pos == std::string::npos ? p.substr(pos2 + 1) :
                                                     p.substr(pos2 + 1, pos - pos2 - 1);
      this->parameters.emplace(std::move(key), std::move(value));
    }
  } else {
    this->path = p;
  }
  
  // extract database prefix /_db/<name>/
  const char* q = this->path.c_str();
  if (this->path.size() >= 4 && q[0] == '/' && q[1] == '_' &&
      q[2] == 'd' && q[3] == 'b' && q[4] == '/') {
    // request contains database name
    q += 5;
    const char* pathBegin = q;
    // read until end of database name
    while (*q != '\0') {
      if (*q == '/' || *q == '?' || *q == ' ' || *q == '\n' ||
          *q == '\r') {
        break;
      }
      ++q;
    }
    this->database = std::string(pathBegin, q - pathBegin);
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

// content-type header accessors
std::string Message::contentTypeString() const {
  return messageHeader().contentTypeString();
}

ContentType Message::contentType() const { return  messageHeader().contentType(); }

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
  
// accept header accessors
std::string Request::acceptTypeString() const {
  return header.acceptTypeString();
}

ContentType Request::acceptType() const { return header.acceptType(); }

//// add payload add VelocyPackData
void Request::addVPack(VPackSlice const& slice) {
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
  if (isContentTypeVPack()) {
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
  return _payload.byteSize() - _payloadOffset;
}
  
std::shared_ptr<velocypack::Buffer<uint8_t>> Response::copyPayload() const {
  auto buffer = std::make_shared<velocypack::Buffer<uint8_t>>();
  buffer->append(_payload.data() + _payloadOffset,
                 _payload.byteSize() - _payloadOffset);
  return buffer;
}
  
void Response::setPayload(VPackBuffer<uint8_t> buffer, size_t payloadOffset) {
  _payloadOffset = payloadOffset;
  _payload = std::move(buffer);
}
}}}  // namespace arangodb::fuerte::v1
