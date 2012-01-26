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
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2006-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_STRING_BUFFER_H
#define TRIAGENS_BASICS_STRING_BUFFER_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// string buffer with formatting routines
// -----------------------------------------------------------------------------

#ifdef STRING_BUFFER_MACROS

#define DEFSTR(a,b) static const char __STRING_ ## a [] = b
#define STR(a) __STRING_ ## a
#define LENSTR(a) (sizeof(__STRING_ ## a) - 1)

#endif

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief string buffer with formatting routines
////////////////////////////////////////////////////////////////////////////////

    struct StringBuffer {

// -----------------------------------------------------------------------------
        // constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the string buffer
///
/// This method is implemented here in order to allow inlining.
///
/// @warning You must call initialise before using the string buffer.
////////////////////////////////////////////////////////////////////////////////

        void initialise () {
          buffer = 0;
          bufferPtr = 0;
          bufferEnd = 0;

          reserve(1);
          *bufferPtr = 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer
///
/// This method is implemented here in order to allow inlining.
///
/// @warning You must call free after using the string buffer.
////////////////////////////////////////////////////////////////////////////////

        void free () {
          if (buffer != 0) {
            delete[] buffer;

            buffer = 0;
            bufferPtr = 0;
            bufferEnd = 0;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the string buffer and cleans the buffer
///
/// This method is implemented here in order to allow inlining.
///
/// @warning You must call free after using the string buffer.
////////////////////////////////////////////////////////////////////////////////

        void destroy () {
          if (buffer != 0) {
            memset(buffer, 0, bufferEnd - buffer);

            delete[] buffer;

            buffer = 0;
            bufferPtr = 0;
            bufferEnd = 0;
          }
        }

// -----------------------------------------------------------------------------
        // public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief swaps content with another string buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void swap (StringBuffer * other) {
          char * otherBuffer    = other->buffer;
          char * otherBufferPtr = other->bufferPtr;
          char * otherBufferEnd = other->bufferEnd;

          other->buffer    = buffer;
          other->bufferPtr = bufferPtr;
          other->bufferEnd = bufferEnd;

          buffer    = otherBuffer;
          bufferPtr = otherBufferPtr;
          bufferEnd = otherBufferEnd;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the character buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        const char * c_str () const {
          return buffer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the beginning of the character buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        const char * begin () const {
          return buffer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns pointer to the end of the character buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        const char * end () const {
          return bufferPtr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns length of the character buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        size_t length () const {
          return bufferPtr - buffer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if buffer is empty
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        bool empty () const {
          return bufferPtr == buffer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void clear () {
          bufferPtr = buffer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief assigns text from a string
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        StringBuffer& operator= (string const& str) {
          replaceText(str.c_str(), str.length());

          return *this;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the string buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void copy (StringBuffer const& source) {
          replaceText(source.c_str(), source.length());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the first characters
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void erase_front (size_t len) {
          if (length() <= len) {
            clear();
          }
          else if (0 < len) {
            memmove(buffer, buffer + len, bufferPtr - buffer - len);
            bufferPtr -= len;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void replaceText (const char * str, size_t len) {
          clear();
          appendText(str, len);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces characters
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void replaceText (StringBuffer const& text) {
          clear();
          appendText(text.c_str(), text.length());
        }

// -----------------------------------------------------------------------------
        // appenders
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appends eol character
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendEol () {
          appendChar('\n');
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends character
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendChar (char chr) {
          reserve(2);
          *bufferPtr++ = chr;
          *bufferPtr   = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendText (const char * str, size_t len) {
          if (len == 0) {
            reserve(1);
            *bufferPtr = '\0';
          }
          else {
            reserve(len + 1);
            memcpy(bufferPtr, str, len);
            bufferPtr += len;
            *bufferPtr = '\0';
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends characters
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendText (const char * str) {
          size_t len = strlen(str);

          reserve(len + 1);
          memcpy(bufferPtr, str, len);
          bufferPtr += len;
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends string
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendText (string const& str) {
          appendText(str.c_str(), str.length());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a string buffer
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendText (StringBuffer const& text) {
          appendText(text.c_str(), text.length());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with two digits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger2 (uint32_t attr) {
          reserve(3);
          appendChar0(char((attr / 10U) % 10 + '0'));
          appendChar0(char( attr % 10        + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with three digits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger3 (uint32_t attr) {
          reserve(4);
          appendChar0(char((attr / 100U) % 10 + '0'));
          appendChar0(char((attr /  10U) % 10 + '0'));
          appendChar0(char( attr         % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with four digits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger4 (uint32_t attr) {
          reserve(5);
          appendChar0(char((attr / 1000U) % 10 + '0'));
          appendChar0(char((attr /  100U) % 10 + '0'));
          appendChar0(char((attr /   10U) % 10 + '0'));
          appendChar0(char( attr          % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 8 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (int8_t attr) {
          if (attr == INT8_MIN) {
            appendText("-128", 4);
            return;
          }

          reserve(5);

          if (attr < 0) {
            appendChar0('-');
            attr = -attr;
          }


          if (100 <= attr) { appendChar0((attr / 100) % 10 + '0'); }
          if ( 10 <= attr) { appendChar0((attr /  10) % 10 + '0'); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 8 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (uint8_t attr) {
          reserve(4);

          if (100U <= attr) { appendChar0((attr / 100U) % 10 + '0'); }
          if ( 10U <= attr) { appendChar0((attr /  10U) % 10 + '0'); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 16 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (int16_t attr) {
          if (attr == INT16_MIN) {
            appendText("-32768", 6);
            return;
          }

          reserve(7);

          if (attr < 0) {
            appendChar0('-');
            attr = -attr;
          }

          if (10000 <= attr) { appendChar0(char((attr / 10000) % 10 + '0')); }
          if ( 1000 <= attr) { appendChar0(char((attr /  1000) % 10 + '0')); }
          if (  100 <= attr) { appendChar0(char((attr /   100) % 10 + '0')); }
          if (   10 <= attr) { appendChar0(char((attr /    10) % 10 + '0')); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (uint16_t attr) {
          reserve(6);

          if (10000U <= attr) { appendChar0(char((attr / 10000U) % 10 + '0')); }
          if ( 1000U <= attr) { appendChar0(char((attr /  1000U) % 10 + '0')); }
          if (  100U <= attr) { appendChar0(char((attr /   100U) % 10 + '0')); }
          if (   10U <= attr) { appendChar0(char((attr /    10U) % 10 + '0')); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 32 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (int32_t attr) {
          if (attr == INT32_MIN) {
            appendText("-2147483648", 11);
            return;
          }

          reserve(12);

          if (attr < 0) {
            appendChar0('-');
            attr = -attr;
          }

          if (1000000000 <= attr) { appendChar0(char((attr / 1000000000) % 10 + '0')); }
          if ( 100000000 <= attr) { appendChar0(char((attr /  100000000) % 10 + '0')); }
          if (  10000000 <= attr) { appendChar0(char((attr /   10000000) % 10 + '0')); }
          if (   1000000 <= attr) { appendChar0(char((attr /    1000000) % 10 + '0')); }
          if (    100000 <= attr) { appendChar0(char((attr /     100000) % 10 + '0')); }
          if (     10000 <= attr) { appendChar0(char((attr /      10000) % 10 + '0')); }
          if (      1000 <= attr) { appendChar0(char((attr /       1000) % 10 + '0')); }
          if (       100 <= attr) { appendChar0(char((attr /        100) % 10 + '0')); }
          if (        10 <= attr) { appendChar0(char((attr /         10) % 10 + '0')); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (uint32_t attr) {
          reserve(11);

          if (1000000000U <= attr) { appendChar0(char((attr / 1000000000U) % 10 + '0')); }
          if ( 100000000U <= attr) { appendChar0(char((attr /  100000000U) % 10 + '0')); }
          if (  10000000U <= attr) { appendChar0(char((attr /   10000000U) % 10 + '0')); }
          if (   1000000U <= attr) { appendChar0(char((attr /    1000000U) % 10 + '0')); }
          if (    100000U <= attr) { appendChar0(char((attr /     100000U) % 10 + '0')); }
          if (     10000U <= attr) { appendChar0(char((attr /      10000U) % 10 + '0')); }
          if (      1000U <= attr) { appendChar0(char((attr /       1000U) % 10 + '0')); }
          if (       100U <= attr) { appendChar0(char((attr /        100U) % 10 + '0')); }
          if (        10U <= attr) { appendChar0(char((attr /         10U) % 10 + '0')); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends integer with 64 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (int64_t attr) {
          if (attr == INT64_MIN) {
            appendText("-9223372036854775808", 20);
            return;
          }
          else if (attr < 0) {
            reserve(1);
            appendChar0('-');
            attr = -attr;
          }

          if ((attr >> 32) == 0) {
            appendInteger(uint32_t(attr));
            return;
          }

          reserve(20);

          // uint64_t has one more decimal than int64_t
          if (1000000000000000000LL <= attr) { appendChar0(char((attr / 1000000000000000000LL) % 10 + '0')); }
          if ( 100000000000000000LL <= attr) { appendChar0(char((attr /  100000000000000000LL) % 10 + '0')); }
          if (  10000000000000000LL <= attr) { appendChar0(char((attr /   10000000000000000LL) % 10 + '0')); }
          if (   1000000000000000LL <= attr) { appendChar0(char((attr /    1000000000000000LL) % 10 + '0')); }
          if (    100000000000000LL <= attr) { appendChar0(char((attr /     100000000000000LL) % 10 + '0')); }
          if (     10000000000000LL <= attr) { appendChar0(char((attr /      10000000000000LL) % 10 + '0')); }
          if (      1000000000000LL <= attr) { appendChar0(char((attr /       1000000000000LL) % 10 + '0')); }
          if (       100000000000LL <= attr) { appendChar0(char((attr /        100000000000LL) % 10 + '0')); }
          if (        10000000000LL <= attr) { appendChar0(char((attr /         10000000000LL) % 10 + '0')); }
          if (         1000000000LL <= attr) { appendChar0(char((attr /          1000000000LL) % 10 + '0')); }
          if (          100000000LL <= attr) { appendChar0(char((attr /           100000000LL) % 10 + '0')); }
          if (           10000000LL <= attr) { appendChar0(char((attr /            10000000LL) % 10 + '0')); }
          if (            1000000LL <= attr) { appendChar0(char((attr /             1000000LL) % 10 + '0')); }
          if (             100000LL <= attr) { appendChar0(char((attr /              100000LL) % 10 + '0')); }
          if (              10000LL <= attr) { appendChar0(char((attr /               10000LL) % 10 + '0')); }
          if (               1000LL <= attr) { appendChar0(char((attr /                1000LL) % 10 + '0')); }
          if (                100LL <= attr) { appendChar0(char((attr /                 100LL) % 10 + '0')); }
          if (                 10LL <= attr) { appendChar0(char((attr /                  10LL) % 10 + '0')); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendInteger (uint64_t attr) {
          if ((attr >> 32) == 0) {
            appendInteger(uint32_t(attr));
            return;
          }

          reserve(21);

          // uint64_t has one more decimal than int64_t
          if (10000000000000000000ULL <= attr) { appendChar0(char((attr / 10000000000000000000ULL) % 10 + '0')); }
          if ( 1000000000000000000ULL <= attr) { appendChar0(char((attr /  1000000000000000000ULL) % 10 + '0')); }
          if (  100000000000000000ULL <= attr) { appendChar0(char((attr /   100000000000000000ULL) % 10 + '0')); }
          if (   10000000000000000ULL <= attr) { appendChar0(char((attr /    10000000000000000ULL) % 10 + '0')); }
          if (    1000000000000000ULL <= attr) { appendChar0(char((attr /     1000000000000000ULL) % 10 + '0')); }
          if (     100000000000000ULL <= attr) { appendChar0(char((attr /      100000000000000ULL) % 10 + '0')); }
          if (      10000000000000ULL <= attr) { appendChar0(char((attr /       10000000000000ULL) % 10 + '0')); }
          if (       1000000000000ULL <= attr) { appendChar0(char((attr /        1000000000000ULL) % 10 + '0')); }
          if (        100000000000ULL <= attr) { appendChar0(char((attr /         100000000000ULL) % 10 + '0')); }
          if (         10000000000ULL <= attr) { appendChar0(char((attr /          10000000000ULL) % 10 + '0')); }
          if (          1000000000ULL <= attr) { appendChar0(char((attr /           1000000000ULL) % 10 + '0')); }
          if (           100000000ULL <= attr) { appendChar0(char((attr /            100000000ULL) % 10 + '0')); }
          if (            10000000ULL <= attr) { appendChar0(char((attr /             10000000ULL) % 10 + '0')); }
          if (             1000000ULL <= attr) { appendChar0(char((attr /              1000000ULL) % 10 + '0')); }
          if (              100000ULL <= attr) { appendChar0(char((attr /               100000ULL) % 10 + '0')); }
          if (               10000ULL <= attr) { appendChar0(char((attr /                10000ULL) % 10 + '0')); }
          if (                1000ULL <= attr) { appendChar0(char((attr /                 1000ULL) % 10 + '0')); }
          if (                 100ULL <= attr) { appendChar0(char((attr /                  100ULL) % 10 + '0')); }
          if (                  10ULL <= attr) { appendChar0(char((attr /                   10ULL) % 10 + '0')); }

          appendChar0(char(attr % 10 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

        void appendInteger (size_t attr) {
          appendInteger(sizetint_t(attr));
        }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in octal
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendOctal (uint32_t attr) {
          reserve(9);

          if (010000000U <= attr) { appendChar0(char((attr / 010000000U) % 010 + '0')); }
          if ( 01000000U <= attr) { appendChar0(char((attr /  01000000U) % 010 + '0')); }
          if (  0100000U <= attr) { appendChar0(char((attr /   0100000U) % 010 + '0')); }
          if (   010000U <= attr) { appendChar0(char((attr /    010000U) % 010 + '0')); }
          if (    01000U <= attr) { appendChar0(char((attr /     01000U) % 010 + '0')); }
          if (     0100U <= attr) { appendChar0(char((attr /      0100U) % 010 + '0')); }
          if (      010U <= attr) { appendChar0(char((attr /       010U) % 010 + '0')); }

          appendChar0(char(attr % 010 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in octal
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendOctal (uint64_t attr) {
          reserve(17);

          if (01000000000000000ULL <= attr) { appendChar0(char((attr / 01000000000000000ULL) % 010 + '0')); }
          if ( 0100000000000000ULL <= attr) { appendChar0(char((attr /  0100000000000000ULL) % 010 + '0')); }
          if (  010000000000000ULL <= attr) { appendChar0(char((attr /   010000000000000ULL) % 010 + '0')); }
          if (   01000000000000ULL <= attr) { appendChar0(char((attr /    01000000000000ULL) % 010 + '0')); }
          if (    0100000000000ULL <= attr) { appendChar0(char((attr /     0100000000000ULL) % 010 + '0')); }
          if (     010000000000ULL <= attr) { appendChar0(char((attr /      010000000000ULL) % 010 + '0')); }
          if (      01000000000ULL <= attr) { appendChar0(char((attr /       01000000000ULL) % 010 + '0')); }
          if (       0100000000ULL <= attr) { appendChar0(char((attr /        0100000000ULL) % 010 + '0')); }
          if (        010000000ULL <= attr) { appendChar0(char((attr /         010000000ULL) % 010 + '0')); }
          if (         01000000ULL <= attr) { appendChar0(char((attr /          01000000ULL) % 010 + '0')); }
          if (          0100000ULL <= attr) { appendChar0(char((attr /           0100000ULL) % 010 + '0')); }
          if (           010000ULL <= attr) { appendChar0(char((attr /            010000ULL) % 010 + '0')); }
          if (            01000ULL <= attr) { appendChar0(char((attr /             01000ULL) % 010 + '0')); }
          if (             0100ULL <= attr) { appendChar0(char((attr /              0100ULL) % 010 + '0')); }
          if (              010ULL <= attr) { appendChar0(char((attr /               010ULL) % 010 + '0')); }

          appendChar0(char(attr % 010 + '0'));
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in octal
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

        void appendOctal (size_t attr) {
          appendOctal(sizetint_t(attr));
        }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 32 bits in hex
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendHex (uint32_t attr) {
          static char const * const HEX = "0123456789ABCDEF";

          reserve(5);

          if (0x1000U <= attr) { appendChar0(HEX[(attr / 0x1000U) % 0x10]); }
          if ( 0x100U <= attr) { appendChar0(HEX[(attr /  0x100U) % 0x10]); }
          if (  0x10U <= attr) { appendChar0(HEX[(attr /   0x10U) % 0x10]); }

          appendChar0(HEX[attr % 0x10]);
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends unsigned integer with 64 bits in hex
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendHex (uint64_t attr) {
          static char const * const HEX = "0123456789ABCDEF";

          reserve(9);

          if (0x10000000U <= attr) { appendChar0(HEX[(attr / 0x10000000U) % 0x10]); }
          if ( 0x1000000U <= attr) { appendChar0(HEX[(attr /  0x1000000U) % 0x10]); }
          if (  0x100000U <= attr) { appendChar0(HEX[(attr /   0x100000U) % 0x10]); }
          if (   0x10000U <= attr) { appendChar0(HEX[(attr /    0x10000U) % 0x10]); }
          if (    0x1000U <= attr) { appendChar0(HEX[(attr /     0x1000U) % 0x10]); }
          if (     0x100U <= attr) { appendChar0(HEX[(attr /      0x100U) % 0x10]); }
          if (      0x10U <= attr) { appendChar0(HEX[(attr /       0x10U) % 0x10]); }

          appendChar0(HEX[attr % 0x10]);
          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends size_t in hex
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

        void appendHex (size_t attr) {
          appendHex(sizetint_t(attr));
        }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief appends floating point number with 8 bits
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendDecimal (double attr) {
          reserve(1);

          if (attr < 0.0) {
            appendChar0('-');
            attr = -attr;
          }
          else if (attr == 0.0) {
            appendChar0('0');
            *bufferPtr = '\0';
            return;
          }

          if (double(uint32_t(attr)) == attr) {
            appendInteger(uint32_t(attr));
            return;
          }
          else if (attr < double(429496U)) {
            uint32_t smll = uint32_t(attr * 10000.0);

            if (double(smll) == attr * 10000.0) {
              uint32_t exp = smll % 10000;

              appendInteger(smll / 10000);

              if (exp != 0) {
                reserve(6);

                appendChar0('.');

                size_t pos = 0;

                if ((exp / 1000L) % 10 != 0)  pos = 1;
                char a1 = char((exp / 1000L) % 10 + '0');

                if ((exp / 100L) % 10 != 0)  pos = 2;
                char a2 = char((exp / 100L) % 10 + '0');

                if ((exp / 10L) % 10 != 0)  pos = 3;
                char a3 = char((exp / 10L) % 10 + '0');

                if (exp % 10 != 0)  pos = 4;
                char a4 = char(exp % 10 + '0');

                appendChar0(a1);
                if (pos > 1) { appendChar0(a2); }
                if (pos > 2) { appendChar0(a3); }
                if (pos > 3) { appendChar0(a4); }

                *bufferPtr = '\0';
              }

              return;
            }
          }

          // we do not habe a small integral number nor small decimal number with only a few decimal digits

          // there at most 16 significant digits, first find out if we have an integer value
          if (10000000000000000.0 < attr) {
            size_t n = 0;

            while (10000000000000000.0 < attr) {
              attr /= 10.0;
              ++n;
            }

            appendInteger((uint64_t) attr);

            reserve(n);

            for (;  0 < n;  --n) {
              appendChar0('0');
            }

            *bufferPtr = '\0';
            return;
          }


          // very small, i. e. less than 1
          else if (attr < 1.0) {
            size_t n = 0;

            while (attr < 1.0) {
              attr *= 10.0;
              ++n;

              // should not happen, so it must be almost 0
              if (n > 400) {
                appendInteger(0);
                return;
              }
            }

            reserve(n + 2);

            appendChar0('0');
            appendChar0('.');

            for (--n;  0 < n;  --n) {
              appendChar0('0');
            }

            attr = 10000000000000000.0 * attr;

            appendInteger((uint64_t) attr);

            return;
          }


          // somewhere in between
          else {
            uint64_t m = uint64_t(attr);
            double d = attr - m;

            appendInteger(m);

            size_t n = 0;

            while (d < 1.0) {
              d *= 10.0;
              ++n;

              // should not happen, so it must be almost 0
              if (n > 400) {
                return;
              }
            }

            reserve(n + 1);

            appendChar0('.');

            for (--n;  0 < n;  --n) {
              appendChar0('0');
            }

            d = 10000000000000000.0 * d;

            appendInteger((uint64_t) d);

            return;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends time in standard format
///
/// This method is implemented here in order to allow inlining.
////////////////////////////////////////////////////////////////////////////////

        void appendTime (int32_t attr) {
          int hour = attr / 3600;
          int minute = (attr / 60) % 60;
          int second = attr % 60;

          reserve(9);

          appendInteger2(hour);
          appendChar(':');
          appendInteger2(minute);
          appendChar(':');
          appendInteger2(second);

          *bufferPtr = '\0';
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv string
////////////////////////////////////////////////////////////////////////////////

        void appendCsvString (string const& text) {
          // do not escape here, because some string - i.e. lists of identifier - have no special characters
          appendText(text);
          appendChar(';');
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv integer
////////////////////////////////////////////////////////////////////////////////

        void appendCsvInteger (int32_t i) {
          appendInteger(i);
          appendChar(';');
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv integer
////////////////////////////////////////////////////////////////////////////////

        void appendCsvInteger (uint32_t i) {
          appendInteger(i);
          appendChar(';');
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv integer
////////////////////////////////////////////////////////////////////////////////

        void appendCsvInteger (uint64_t i) {
          appendInteger(i);
          appendChar(';');
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends csv double
////////////////////////////////////////////////////////////////////////////////

        void appendCsvDouble (double d) {
          appendDecimal(d);
          appendChar(';');
        }

// -----------------------------------------------------------------------------
        // private methods
// -----------------------------------------------------------------------------

        // private
        void appendChar0 (char chr) {
          *bufferPtr++ = chr;
        }

        void reserve (size_t size) {
          if (buffer == 0) {
            buffer = new char[size + 1];
            bufferPtr = buffer;
            bufferEnd = buffer + size;
          }
          else if (size_t(bufferEnd - bufferPtr) < size) {
            size_t newlen = size_t(1.2 * ((bufferEnd - buffer) + size));
            char * b = new char[newlen + 1];

            memcpy(b, buffer, bufferEnd - buffer + 1);

            delete[] buffer;

            bufferPtr = b + (bufferPtr - buffer);
            bufferEnd = b + newlen;
            buffer    = b;
          }
        }

// -----------------------------------------------------------------------------
        // private data
// -----------------------------------------------------------------------------

        char * buffer;
        char * bufferPtr;
        char * bufferEnd;
    };
  }
}

#endif
