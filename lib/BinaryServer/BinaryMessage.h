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

      private:

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
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the protocol signature
////////////////////////////////////////////////////////////////////////////////

        static const char* getSignature () {
          return "\xaa\xdb\x00\x00";
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the encoded length as a size_t 
////////////////////////////////////////////////////////////////////////////////

        static uint32_t decodeLength (const uint8_t* data) {
          uint32_t length = ((uint32_t) data[0] << 24) |
                            ((uint32_t) data[1] << 16) |
                            ((uint32_t) data[2] << 8) |
                            (uint32_t) data[3];

          return length;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief encode the size_t length in a char[4]
////////////////////////////////////////////////////////////////////////////////

        static void writeHeader (const uint32_t length, uint8_t* out) {
          static const char* signature = getSignature();

          for (size_t i = 0; i < 4; ++i) {
            *(out++) = (uint8_t) signature[i];
          }

          *(out++) = ((uint32_t) length >> 24);
          *(out++) = ((uint32_t) length >> 16) & 0xff;
          *(out++) = ((uint32_t) length >> 8) & 0xff;
          *(out++) = ((uint32_t) length) & 0xff;
        }

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

