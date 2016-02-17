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
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_REST_VELOCY_RESPONSE_H
#define LIB_REST_VELOCY_RESPONSE_H 1

#include "Basics/Common.h"

#include "Basics/Dictionary.h"
#include "Basics/StringBuffer.h"

#include "Rest/ArangoResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief vstream response
///
/// A vstream request handler is called to handle a vstream request. It returns its
/// answer as vstream response.
////////////////////////////////////////////////////////////////////////////////

class VelocyResponse: public ArangoResponse {

    VelocyResponse() = delete;
    VelocyResponse(VelocyResponse const&) = delete;
    VelocyResponse& operator=(VelocyResponse const&) = delete;

  public:

    VelocyResponse(ResponseCode, uint32_t);

    //////////////////////////////////////////////////////////////////////////////
    /// @brief deletes a vstream response
    ///
    /// The destructor will free the string buffers used. After the vstream response
    /// is deleted, the string buffers returned by body() become invalid.
    //////////////////////////////////////////////////////////////////////////////

    ~VelocyResponse();  
  public:
  
    //////////////////////////////////////////////////////////////////////////////
    /// @brief writes the header (Vstream)
    //////////////////////////////////////////////////////////////////////////////

    arangodb::velocypack::Builder writeHeader(arangodb::velocypack::Builder*); 

    //////////////////////////////////////////////////////////////////////////////
    /// @brief returns the body
    ///
    /// Returns a reference to the body. This reference is only valid as long as
    /// vstream response exists. You can add data to the body by appending
    /// information to the string buffer. Note that adding data to the body
    /// invalidates any previously returned header. You must call header
    /// again.
    //////////////////////////////////////////////////////////////////////////////

    arangodb::velocypack::Builder& body(); //@vstream for vstream

    //////////////////////////////////////////////////////////////////////////////
    /// @brief indicates a head response
    ///
    /// In case of HEAD request, no body must be defined. However, the response
    /// needs to know the size of body.
    //////////////////////////////////////////////////////////////////////////////

    void headResponse(size_t);

    //////////////////////////////////////////////////////////////////////////////
    /// @brief handling status response's (Vstream) //@vstream for vstream
    ///
    //////////////////////////////////////////////////////////////////////////////

    void statusResponse(size_t);


    //////////////////////////////////////////////////////////////////////////////
    /// @brief deflates the response body
    ///
    /// the body must already be set. deflate is then run on the existing body
    //////////////////////////////////////////////////////////////////////////////

    int deflate(size_t = 16384);

    //////////////////////////////////////////////////////////////////////////////
    /// @brief checks if the given packet is first chunked (velocystream)
    //////////////////////////////////////////////////////////////////////////////

    bool isFirstChunked() const; // @vstream

  protected:
    
    //////////////////////////////////////////////////////////////////////////////
    /// @brief body
    //////////////////////////////////////////////////////////////////////////////

    arangodb::velocypack::Builder _body;
};
}
} 

#endif