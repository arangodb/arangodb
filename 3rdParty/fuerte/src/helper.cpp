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
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <sstream>
#include <stdexcept>

#include "http.h"

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
  // ss << "Id:" << message.messageID << "\n";
  ss << "Header:\n";
  if (message.type() == MessageType::Request) {
    Request const& req = static_cast<Request const&>(message);

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
      ss << "\t" << fu_content_type_key << " -:- "
         << to_string(req.header.contentType()) << "\n";
      ss << "\t" << fu_accept_key << " -:- "
         << to_string(req.header.acceptType()) << "\n";
      for (auto const& item : req.header.meta()) {
        ss << "\t" << item.first << " -:- " << item.second << "\n";
      }
      ss << std::endl;
    }
  } else if (message.type() == MessageType::Response) {
    Response const& res = static_cast<Response const&>(message);

    ss << "type: response" << std::endl;
    if (res.header.responseCode != StatusUndefined) {
      ss << "responseCode: " << res.header.responseCode << std::endl;
    }

    if (!res.header.meta().empty()) {
      ss << "meta:\n";
      ss << "\t" << fu_content_type_key << " -:- "
         << to_string(res.header.contentType()) << "\n";
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

std::string extractPathParameters(std::string_view p, StringMap& params) {
  size_t pos = p.rfind('?');
  if (pos == p.npos) {
    return std::string(p);
  }

  std::string result(p.substr(0, pos));

  while (pos != p.npos && pos + 1 < p.length()) {
    size_t pos2 = p.find('=', pos + 1);
    if (pos2 == p.npos) {
      break;
    }
    std::string_view key = p.substr(pos + 1, pos2 - pos - 1);
    pos = p.find('&', pos2 + 1);  // points to next '&' or string::npos
    std::string_view value =
        pos == p.npos ? p.substr(pos2 + 1) : p.substr(pos2 + 1, pos - pos2 - 1);
    params.emplace(http::urlDecode(key), http::urlDecode(value));
  }

  return result;
}

}}}  // namespace arangodb::fuerte::v1
