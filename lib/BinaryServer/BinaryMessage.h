////////////////////////////////////////////////////////////////////////////////
/// @brief binary protocol data container
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BINARY_SERVER_BINARY_MESSAGE_H
#define TRIAGENS_BINARY_SERVER_BINARY_MESSAGE_H 1

#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                               class BinaryMessage
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief message used for binary communication
////////////////////////////////////////////////////////////////////////////////

    class BinaryMessage {

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

        BinaryMessage ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructors
////////////////////////////////////////////////////////////////////////////////

        ~BinaryMessage ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the required content-type string
////////////////////////////////////////////////////////////////////////////////

        static string const& getContentType () {
          static string const contentType = "application/x-arangodb-batch"; 

          return contentType;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the minimum binary message length
////////////////////////////////////////////////////////////////////////////////

        static const size_t getHeaderLength () {
          return 8;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the maximum binary message length
////////////////////////////////////////////////////////////////////////////////
        
        static const size_t getMaxLength () {
          return 128 * 1024 * 1024;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief validate the message
////////////////////////////////////////////////////////////////////////////////

        bool validate (const char* data, const char* end, size_t* bodyLength) {
          size_t length = end - data;

          // validate message container
          if (end <= data) {
            return false;
          }

          // validate message length
          if (length < getHeaderLength()) {
            return false;
          }

          // validate signature
          if ((data[0] & 0xff) != 0xaa) {
            return false;
          }

          if ((data[1] & 0xff) != 0xdb) {
            return false;
          }

          // validate body length
          *bodyLength = (size_t) data[7] + 
                        (size_t) (data[6] << 8) + 
                        (size_t) (data[5] << 16) +
                        (size_t) (data[4] << 24);

          if (*bodyLength > getMaxLength()) {
            LOGGER_WARNING << "maximum binary message size is " << getMaxLength() << ", actual size is " << *bodyLength;
            return false;
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:

