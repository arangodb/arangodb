////////////////////////////////////////////////////////////////////////////////
/// @brief line response
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_LINE_RESPONSE_H
#define TRIAGENS_REST_LINE_RESPONSE_H 1

#include "Basics/Common.h"

#include "Basics/StringBuffer.h"
#include "Rest/LineRequest.h"


namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief line response
///
/// A line request handler is called to handle a line request. It returns its
/// answer as line response.
////////////////////////////////////////////////////////////////////////////////

    class LineRequest;

    class  LineResponse : noncopyable {
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief line response codes
////////////////////////////////////////////////////////////////////////////////

        enum LineResponseCode {
          OK,
          ERROR_CODE
        };

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        LineResponse () : _lineRequest(0) {
        }

        LineResponse (const char* header, size_t headerLength) : _lineRequest(0) {
          _headerValue.appendText(header,headerLength);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a line response
///
/// The descrutor will free the string buffers used. After the line response
/// is deleted, the string buffers returned by body() become invalid.
////////////////////////////////////////////////////////////////////////////////

        virtual ~LineResponse () {
          if (_lineRequest != 0) {
            delete _lineRequest;
          }
        }

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the body
////////////////////////////////////////////////////////////////////////////////

        basics::StringBuffer& body () {
          return _bodyValue;
        }

        basics::StringBuffer& header () {
          return _headerValue;
        }

        LineRequest* request () {
          return _lineRequest;
        }


      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////

        virtual const size_t& bodyLength () const {
          return _bodyLength;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates the response
////////////////////////////////////////////////////////////////////////////////

        virtual void write (basics::StringBuffer* output) {
          output->copy(_bodyValue);
        }

      protected:

        basics::StringBuffer _headerValue;

        basics::StringBuffer _bodyValue;

        size_t _bodyLength;

        LineRequest* _lineRequest;

    };
  }
}

#endif
