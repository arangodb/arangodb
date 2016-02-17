////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "VelocyResponse.h"
#include "Basics/logging.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"

#include "Rest/ArangoResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new Vstream response
////////////////////////////////////////////////////////////////////////////////

VelocyResponse::VelocyResponse(ArangoResponse::ResponseCode cde, uint32_t apiC)
    : ArangoResponse(cde, apiC),
      _body() {
}

VelocyResponse::~VelocyResponse(){

}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the header
////////////////////////////////////////////////////////////////////////////////

arangodb::velocypack::Builder VelocyResponse::writeHeader(arangodb::velocypack::Builder* vobject) {

  arangodb::velocypack::Builder b; // { VPackObjectBuilder bb(&b, "parameter");//   b.add("a", Value("1");// }  
  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  bool seenTransferEncoding = false;
  std::string transferEncoding;
  bool const capitalizeHeaders = (_apiCompatibility >= 20100);
  b.add(arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object));
  b.add("version", arangodb::velocypack::Value("VSTREAM_1_0"));
  b.add("code", arangodb::velocypack::Value(_code));
  for (_headers.range(begin, end); begin < end; ++begin) {
    char const* key = begin->_key;

    if (key == nullptr) {
      continue;
    }

    size_t const keyLength = strlen(key);

    if (capitalizeHeaders) {
      // @TODO: Revaluate this  => ::toupper
      b.add(key, arangodb::velocypack::Value(begin->_value));
    } else {
      b.add(key, arangodb::velocypack::Value(begin->_value));
    }
  }

  for (std::vector<char const*>::iterator iter = _cookies.begin();
       iter != _cookies.end(); ++iter) {
    if (capitalizeHeaders) {
      b.add("Set-Cookie: ", arangodb::velocypack::Value(*iter));
    } else {
      b.add("set-cookie: ", arangodb::velocypack::Value(*iter));
    }
  }

  // Size of Entire Document, not just current chunk
    if (capitalizeHeaders) {
      if (_isHeadResponse) {
        b.add("Content-Size: ", arangodb::velocypack::Value(_bodySize));
      }else{
        b.add("Content-Size: ", arangodb::velocypack::Value(arangodb::velocypack::Slice(_body.start()).byteSize()));
      }
    } else {
      if (_isHeadResponse) {
        b.add("content-size: ", arangodb::velocypack::Value(_bodySize));
      }else{
        b.add("content-size: ", arangodb::velocypack::Value(arangodb::velocypack::Slice(_body.start()).byteSize()))
        ;
      }
    }
  b.close();  
  return b;

}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates a head response
////////////////////////////////////////////////////////////////////////////////

void VelocyResponse::headResponse(size_t size) {
  _body.clear();
  _isHeadResponse = true;
  _bodySize = size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the body
////////////////////////////////////////////////////////////////////////////////

arangodb::velocypack::Builder& VelocyResponse::body() { return _body; }