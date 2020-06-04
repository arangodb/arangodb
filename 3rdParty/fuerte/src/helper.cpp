////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/helper.h>

#include <string.h>
#include <sstream>
#include <stdexcept>

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb { namespace fuerte { inline namespace v1 {

std::string to_string(VPackSlice const& slice) {
  std::stringstream ss;
  try {
    std::string json = slice.toJson();
    ss << json << ", " << slice.byteSize() << ", " << json.length();
  } catch (std::exception& e) {
    ss << "slice to string failed with: " << e.what();
  }
  ss << std::endl;

  return ss.str();
}

std::string to_string(std::vector<VPackSlice> const& slices) {
  std::stringstream ss;
  if (!slices.empty()) {
    for (auto const& slice : slices) {
      ss << to_string(slice);
    }
  } else {
    ss << "empty";
  }
  ss << "\n";
  return ss.str();
}

// message is not const because message.slices is not
std::string to_string(Message& message) {
  std::stringstream ss;
  ss << "\n#### Message #####################################\n";
  //ss << "Id:" << message.messageID << "\n";
  ss << "Header:\n";
  if (message.type() == MessageType::Request) {
    Request const& req = static_cast<Request const&>(message);
    
    if (req.header.version()) {
      ss << "version: " << req.header.version() << std::endl;
    }
    
    ss << "type: request" << std::endl;
    
    if (!req.header.database.empty()) {
      ss << "database: " << req.header.database << std::endl;
    }
    
    if (req.header.restVerb != RestVerb::Illegal) {
      ss << "restVerb: " << to_string(req.header.restVerb) << std::endl;
    }
    
    if (!req.header.path.empty()) {
      ss << "path: " << req.header.path << std::endl;
    }
    
    if (!req.header.parameters.empty()) {
      ss << "parameters: ";
      for (auto const& item : req.header.parameters) {
        ss << item.first << " -:- " << item.second << "\n";
      }
      ss << std::endl;
    }

    if (!req.header.meta().empty()) {
      ss << "meta:\n";
      ss << "\t" << fu_content_type_key << " -:- " << to_string(req.header.contentType()) << "\n";
      ss << "\t" << fu_accept_key << " -:- " << to_string(req.header.acceptType()) << "\n";
      for (auto const& item : req.header.meta()) {
        ss << "\t" << item.first << " -:- " << item.second << "\n";
      }
      ss << std::endl;
    }
  } else if (message.type() == MessageType::Response) {
    Response const& res = static_cast<Response const&>(message);
    
    if (res.header.version()) {
      ss << "version: " << res.header.version() << std::endl;
    }
    
    ss << "type: response" << std::endl;
    if (res.header.responseCode != StatusUndefined) {
      ss << "responseCode: " << res.header.responseCode << std::endl;
    }

    if (!res.header.meta().empty()) {
      ss << "meta:\n";
      ss << "\t" << fu_content_type_key << " -:- " << to_string(res.header.contentType()) << "\n";
      for (auto const& item : res.header.meta()) {
        ss << "\t" << item.first << " -:- " << item.second << "\n";
      }
      ss << std::endl;
    }

    ss << "contentType: " << to_string(res.header.contentType()) << std::endl;
  }
  /*  if (header.user) {
   ss << "user: " << header.user.get() << std::endl;
   }
   
   if (header.password) {
   ss << "password: " << header.password.get() << std::endl;
   }*/
  ss << "\nBody:\n";
  if (message.contentType() == ContentType::VPack) {
    ss << to_string(message.slices());
  } else {
    ss << message.payloadAsString();
  }
  ss << "\n";
  ss << "##################################################\n";
  return ss.str();
}

// .............................................................................
// BASE64
// .............................................................................

char const* const BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string encodeBase64(std::string const& in, bool pad) {
  unsigned char charArray3[3];
  unsigned char charArray4[4];

  std::string ret;
  ret.reserve((in.size() * 4 / 3) + 2);

  int i = 0;

  unsigned char const* bytesToEncode =
      reinterpret_cast<unsigned char const*>(in.c_str());
  size_t in_len = in.size();

  while (in_len--) {
    charArray3[i++] = *(bytesToEncode++);

    if (i == 3) {
      charArray4[0] = (charArray3[0] & 0xfc) >> 2;
      charArray4[1] =
          ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
      charArray4[2] =
          ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
      charArray4[3] = charArray3[2] & 0x3f;

      for (i = 0; i < 4; i++) {
        ret += BASE64_CHARS[charArray4[i]];
      }

      i = 0;
    }
  }

  if (i != 0) {
    for (int j = i; j < 3; j++) {
      charArray3[j] = '\0';
    }

    charArray4[0] = (charArray3[0] & 0xfc) >> 2;
    charArray4[1] =
        ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
    charArray4[2] =
        ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
    charArray4[3] = charArray3[2] & 0x3f;

    for (int j = 0; (j < i + 1); j++) {
      ret += BASE64_CHARS[charArray4[j]];
    }

    if (pad) {
      while ((i++ < 3)) {
        ret += '=';
      }
    }
  }

  return ret;
}

std::string encodeBase64U(std::string const& in, bool pad) {
  std::string encoded = encodeBase64(in, pad);
  // replace '+', '/' with '-' and '_'
  std::replace(encoded.begin(), encoded.end(), '+', '-');
  std::replace(encoded.begin(), encoded.end(), '/', '_');
  return encoded;
}

void toLowerInPlace(std::string& str) {
  // unrolled version of
  // for (auto& c : str) {
  //   c = StringUtils::tolower(c);
  // }
  auto tl = [](char c) -> char {
    return c + ((static_cast<unsigned char>(c - 65) < 26U) << 5);
  };
  
  auto pos = str.data();
  auto end = pos + str.size();

  while (pos != end) {
    size_t len = static_cast<size_t>(end - pos);
    if (len > 4) {
      len = 4;
    }

    switch (len) {
      case 4:
        pos[3] = tl(pos[3]);
        [[fallthrough]];
      case 3:
        pos[2] = tl(pos[2]);
        [[fallthrough]];
      case 2:
        pos[1] = tl(pos[1]);
        [[fallthrough]];
      case 1:
        pos[0] = tl(pos[0]);
    }
    pos += len;
  }
}
  
fuerte::Error translateError(asio_ns::error_code e, fuerte::Error c) {
  
  if (e == asio_ns::error::misc_errors::eof ||
      e == asio_ns::error::connection_reset) {
    return fuerte::Error::ConnectionClosed;
  } else if (e == asio_ns::error::operation_aborted) {
    return fuerte::Error::Canceled;
  }
  
  return c;
}
  
}}}  // namespace arangodb::fuerte::v1
