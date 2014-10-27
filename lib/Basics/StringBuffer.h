////////////////////////////////////////////////////////////////////////////////
/// @brief string buffer with formatting routines
///
/// @file
///
/// @warning You must initialise the classes by calling initialise or by
/// setting everything to 0. This can be done by using "new
/// StringBuffer()". You must call free to free the allocated
/// memory.
///
/// WARNING: this must be a POD object!
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_STRING_BUFFER_H
#define ARANGODB_BASICS_STRING_BUFFER_H 1

#include "Basics/Common.h"

#include "Basics/string-buffer.h"
#include "Zip/zip.h"

// -----------------------------------------------------------------------------
// string buffer with formatting routines
// -----------------------------------------------------------------------------

#ifdef STRING_BUFFER_MACROS

#define STR(a) __STRING_ ## a

#endif

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                      class Logger
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief string buffer with formatting routines
////////////////////////////////////////////////////////////////////////////////

    class StringBuffer {
      private:
        StringBuffer (StringBuffer const&);
        StringBuffer& operator= (StringBuffer const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the string buffer
////////////////////////////////////////////////////////////////////////////////

        explicit
        StringBuffer (TRI_memory_zone_t* zone) {
          TRI_InitStringBuffer(&_buffer, zone);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the string buffer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer (TRI_memory_zone_t* zone, const size_t initialSize) {
          TRI_InitSizedStringBuffer(&_buffer, zone, initialSize);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer
////////////////////////////////////////////////////////////////////////////////

        ~StringBuffer () {
          TRI_DestroyStringBuffer(&_buffer);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer and cleans the buffer
////////////////////////////////////////////////////////////////////////////////

        void annihilate () {
          TRI_AnnihilateStringBuffer(&_buffer);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure the string buffer has a specific capacity
////////////////////////////////////////////////////////////////////////////////

        int reserve (size_t length) {
          return TRI_ReserveStringBuffer(&_buffer, length);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief compress the buffer using deflate
////////////////////////////////////////////////////////////////////////////////

        int deflate (size_t bufferSize) {
          return TRI_DeflateStringBuffer(&_buffer, bufferSize);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief uncompress the buffer into stringstream out, using zlib-inflate
////////////////////////////////////////////////////////////////////////////////

        int inflate (std::stringstream& out,
                     size_t bufferSize = 16384) {
          z_stream strm;

          strm.zalloc   = Z_NULL;
          strm.zfree    = Z_NULL;
          strm.opaque   = Z_NULL;
          strm.avail_in = 0;
          strm.next_in  = Z_NULL;

          int res = inflateInit(&strm);

          if (res != Z_OK) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          char* buffer = new char[bufferSize];

          if (buffer == 0) {
            (void) inflateEnd(&strm);

            return TRI_ERROR_OUT_OF_MEMORY;
          }

          strm.avail_in = (int) this->length();
          strm.next_in = (unsigned char*) this->c_str();

          do {
            if (strm.avail_in == 0) {
              break;
            }

            do {
              strm.avail_out = (uInt) bufferSize;
              strm.next_out  = (unsigned char*) buffer;

              res = ::inflate(&strm, Z_NO_FLUSH);
              TRI_ASSERT(res != Z_STREAM_ERROR);

              switch (res) {
                case Z_NEED_DICT:
                case Z_DATA_ERROR:
                case Z_MEM_ERROR: {
                  (void) inflateEnd(&strm);
                  delete[] buffer;

                  return TRI_ERROR_INTERNAL;
                }
              }

              out.write(buffer, bufferSize - strm.avail_out);
            }
            while (strm.avail_out == 0);
          }
          while (res != Z_STREAM_END);

          (void) inflateEnd(&strm);
          delete[] buffer;

          if (res != Z_STREAM_END) {
            return TRI_ERROR_NO_ERROR;
          }

          return TRI_ERROR_INTERNAL;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief uncompress the buffer into StringBuffer out, using zlib-inflate
////////////////////////////////////////////////////////////////////////////////

        int inflate (triagens::basics::StringBuffer& out,
                     size_t bufferSize = 16384) {
          z_stream strm;

          strm.zalloc   = Z_NULL;
          strm.zfree    = Z_NULL;
          strm.opaque   = Z_NULL;
          strm.avail_in = 0;
          strm.next_in  = Z_NULL;

          int res = inflateInit(&strm);

          if (res != Z_OK) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }

          char* buffer = new char[bufferSize];

          if (buffer == 0) {
            (void) inflateEnd(&strm);

            return TRI_ERROR_OUT_OF_MEMORY;
          }

          strm.avail_in = (int) this->length();
          strm.next_in = (unsigned char*) this->c_str();

          do {
            if (strm.avail_in == 0) {
              break;
            }

            do {
              strm.avail_out = (uInt) bufferSize;
              strm.next_out  = (unsigned char*) buffer;

              res = ::inflate(&strm, Z_NO_FLUSH);
              TRI_ASSERT(res != Z_STREAM_ERROR);

              switch (res) {
                case Z_NEED_DICT:
                case Z_DATA_ERROR:
                case Z_MEM_ERROR: {
                  (void) inflateEnd(&strm);
                  delete[] buffer;

                  return TRI_ERROR_INTERNAL;
                }
              }

              out.appendText(buffer, bufferSize - strm.avail_out);
            }
            while (strm.avail_out == 0);
          }
          while (res != Z_STREAM_END);

          (void) inflateEnd(&strm);
          delete[] buffer;

          if (res != Z_STREAM_END) {
            return TRI_ERROR_NO_ERROR;
          }

          return TRI_ERROR_INTERNAL;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the low level buffer
////////////////////////////////////////////////////////////////////////////////

        TRI_string_buffer_t* stringBuffer () {
          return &_buffer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief swaps content with another string buffer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& swap (StringBuffer * other) {
          TRI_SwapStringBuffer(&_buffer, &other->_buffer);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the character buffer
////////////////////////////////////////////////////////////////////////////////

        const char * c_str () const {
          return TRI_BeginStringBuffer(&_buffer);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the beginning of the character buffer
////////////////////////////////////////////////////////////////////////////////

        const char * begin () const {
          return TRI_BeginStringBuffer(&_buffer);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the end of the character buffer
////////////////////////////////////////////////////////////////////////////////

        const char * end () const {
          return TRI_EndStringBuffer(&_buffer);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the end of the character buffer
////////////////////////////////////////////////////////////////////////////////

        char * end () {
          return const_cast<char*>(TRI_EndStringBuffer(&_buffer));
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns length of the character buffer
////////////////////////////////////////////////////////////////////////////////

        size_t length () const {
          return TRI_LengthStringBuffer(&_buffer);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief increases length of the character buffer
////////////////////////////////////////////////////////////////////////////////

        void increaseLength (size_t n) {
          TRI_IncreaseLengthStringBuffer(&_buffer, n);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if buffer is empty
////////////////////////////////////////////////////////////////////////////////

        bool empty () const {
          return TRI_EmptyStringBuffer(&_buffer);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief resets the buffer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& reset () {
          TRI_ResetStringBuffer(&_buffer);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the buffer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& clear () {
          TRI_ClearStringBuffer(&_buffer);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief assigns text from a string
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& operator= (std::string const& str) {
          TRI_ReplaceStringStringBuffer(&_buffer, str.c_str(), str.length());
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the string buffer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& copy (StringBuffer const& source) {
          TRI_CopyStringBuffer(&_buffer, &source._buffer);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the first characters and clears the remaining buffer space
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& erase_front (size_t len) {
          TRI_EraseFrontStringBuffer(&_buffer, len);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the first characters but does not clear the remaining
/// buffer space
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& move_front (size_t len) {
          TRI_MoveFrontStringBuffer(&_buffer, len);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& replaceText (const char * str, size_t len) {
          TRI_ReplaceStringStringBuffer(&_buffer, str, len);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& replaceText (StringBuffer const& text) {
          TRI_ReplaceStringStringBuffer(&_buffer, text.c_str(), text.length());
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the buffer content
////////////////////////////////////////////////////////////////////////////////

        void set (TRI_string_buffer_t const* other) {
          if (_buffer._buffer != 0) {
            TRI_Free(_buffer._memoryZone, _buffer._buffer);
          }

          _buffer._memoryZone = other->_memoryZone;
          _buffer._buffer     = other->_buffer;
          _buffer._current    = other->_current;
          _buffer._len        = other->_len;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                    STRING AND CHARACTER APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends eol character
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendEol () {
          TRI_AppendCharStringBuffer(&_buffer, '\n');
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends character
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendChar (char chr) {
          TRI_AppendCharStringBuffer(&_buffer, chr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends as json-encoded
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendJsonEncoded (const char * str) {
          TRI_AppendJsonEncodedStringStringBuffer(&_buffer, str, true);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendText (const char * str, size_t len) {
          TRI_AppendString2StringBuffer(&_buffer, str, len);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendText (const char * str) {
          TRI_AppendStringStringBuffer(&_buffer, str);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends string
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendText (std::string const& str) {
          TRI_AppendString2StringBuffer(&_buffer, str.c_str(), str.length());
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string buffer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendText (StringBuffer const& text) {
          TRI_AppendString2StringBuffer(&_buffer, text.c_str(), text.length());
          return *this;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 INTEGER APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with two digits
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger2 (uint32_t attr) {
          TRI_AppendInteger2StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with three digits
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger3 (uint32_t attr) {
          TRI_AppendInteger3StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with four digits
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger4 (uint32_t attr) {
          TRI_AppendInteger4StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 8 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (int8_t attr) {
          TRI_AppendInt8StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 8 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (uint8_t attr) {
          TRI_AppendUInt8StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 16 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (int16_t attr) {
          TRI_AppendInt16StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (uint16_t attr) {
          TRI_AppendUInt16StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 32 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (int32_t attr) {
          TRI_AppendInt32StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (uint32_t attr) {
          TRI_AppendUInt32StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 64 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (int64_t attr) {
          TRI_AppendInt64StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendInteger (uint64_t attr) {
          TRI_AppendUInt64StringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

        StringBuffer& appendInteger (size_t attr) {
          return appendInteger(sizetint_t(attr));
        }

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                           INTEGER OCTAL APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in octal
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendOctal (uint32_t attr) {
          TRI_AppendUInt32OctalStringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in octal
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendOctal (uint64_t attr) {
          TRI_AppendUInt64OctalStringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in octal
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

        StringBuffer& appendOctal (size_t attr) {
          return appendOctal(sizetint_t(attr));
        }

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                             INTEGER HEX APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in hex
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendHex (uint32_t attr) {
          TRI_AppendUInt32HexStringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in hex
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendHex (uint64_t attr) {
          TRI_AppendUInt64HexStringBuffer(&_buffer, attr);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in hex
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

        StringBuffer& appendHex (size_t attr) {
          return appendHex(sizetint_t(attr));
        }

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   FLOAT APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends floating point number with 8 bits
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendDecimal (double attr) {
          TRI_AppendDoubleStringBuffer(&_buffer, attr);
          return *this;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                           DATE AND TIME APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends time in standard format
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendTime (int32_t attr) {
          TRI_AppendTimeStringBuffer(&_buffer, attr);
          return *this;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                     CSV APPENDERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv string
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendCsvString (std::string const& text) {

          // do not escape here, because some string - i.e. lists of identifier - have no special characters
          TRI_AppendString2StringBuffer(&_buffer, text.c_str(), text.size());
          TRI_AppendCharStringBuffer(&_buffer, ';');

          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv 32-bit integer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendCsvInteger (int32_t i) {
          TRI_AppendCsvInt32StringBuffer(&_buffer, i);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv unisgned 32-bit integer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendCsvInteger (uint32_t i) {
          TRI_AppendCsvUInt32StringBuffer(&_buffer, i);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv 64-bit integer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendCsvInteger (int64_t i) {
          TRI_AppendCsvInt64StringBuffer(&_buffer, i);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv unsigned 64-bit integer
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendCsvInteger (uint64_t i) {
          TRI_AppendCsvUInt64StringBuffer(&_buffer, i);
          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv double
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& appendCsvDouble (double d) {
          TRI_AppendCsvDoubleStringBuffer(&_buffer, d);
          return *this;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying C string buffer
////////////////////////////////////////////////////////////////////////////////

        TRI_string_buffer_t _buffer;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
