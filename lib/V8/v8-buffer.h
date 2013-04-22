////////////////////////////////////////////////////////////////////////////////
/// @brief binary buffer
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
///
/// Parts of the code are based on:
///
/// Copyright Joyent, Inc. and other Node contributors.
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the
/// "Software"), to deal in the Software without restriction, including
/// without limitation the rights to use, copy, modify, merge, publish,
/// distribute, sublicense, and/or sell copies of the Software, and to permit
/// persons to whom the Software is furnished to do so, subject to the
/// following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
/// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
/// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
/// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
/// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
/// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
/// USE OR OTHER DEALINGS IN THE SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_V8_BUFFER_H
#define TRIAGENS_V8_V8_BUFFER_H 1

#include "V8/v8-wrapper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer encoding
////////////////////////////////////////////////////////////////////////////////

typedef enum TRI_V8_encoding_e {
  ASCII, UTF8, BASE64, UCS2, BINARY, HEX, BUFFER
}
TRI_V8_encoding_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                   class V8Wrapper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief binary buffer
///
/// A buffer is a chunk of memory stored outside the V8 heap, mirrored by an
/// object in javascript. The object is not totally opaque, one can access
/// individual bytes with [] and slice it into substrings or sub-buffers without
/// copying memory.
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_BUFFER_CID (0xF000)

class V8Buffer : public V8Wrapper<V8Buffer, TRI_V8_BUFFER_CID> {

// -----------------------------------------------------------------------------
// --SECTION--                                           static public variables
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal length
///
/// mirrors deps/v8/src/objects.h
////////////////////////////////////////////////////////////////////////////////

    static const unsigned int kMaxLength = 0x3fffffff;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the object is an instance
////////////////////////////////////////////////////////////////////////////////

    static bool hasInstance (v8::Handle<v8::Value> val);

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief the buffer data for a handle
////////////////////////////////////////////////////////////////////////////////

    static inline char* data (v8::Handle<v8::Value> val) {
      assert(val->IsObject());

      void* data = val.As<v8::Object>()->GetIndexedPropertiesExternalArrayData();
      return static_cast<char*>(data);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief the buffer data
////////////////////////////////////////////////////////////////////////////////

    static inline char* data (V8Buffer *b) {
      return V8Buffer::data(b->_handle);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief length of the data for a handle
////////////////////////////////////////////////////////////////////////////////

    static inline size_t length (v8::Handle<v8::Value> val) {
      assert(val->IsObject());

      int len = val.As<v8::Object>()->GetIndexedPropertiesExternalArrayDataLength();
      return static_cast<size_t>(len);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief length of the data
////////////////////////////////////////////////////////////////////////////////

    static inline size_t length (V8Buffer *b) {
      return V8Buffer::length(b->_handle);
    }

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief free callback type
////////////////////////////////////////////////////////////////////////////////

    typedef void (*free_callback_fptr)(char *data, void *hint);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief instance constructor
////////////////////////////////////////////////////////////////////////////////

    static v8::Handle<v8::Value> New (const v8::Arguments &args);

////////////////////////////////////////////////////////////////////////////////
/// @brief C++ API for constructing fast buffer
////////////////////////////////////////////////////////////////////////////////

    static v8::Handle<v8::Object> New (v8::Handle<v8::String> string);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    static V8Buffer* New (size_t length);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, data is copied
////////////////////////////////////////////////////////////////////////////////

    static V8Buffer* New (const char *data, size_t length);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with free callback
////////////////////////////////////////////////////////////////////////////////

    static V8Buffer* New (char *data,
                          size_t length,
                          free_callback_fptr callback,
                          void *hint);

////////////////////////////////////////////////////////////////////////////////
/// @brief desctructor
////////////////////////////////////////////////////////////////////////////////

    ~V8Buffer ();

////////////////////////////////////////////////////////////////////////////////
/// @brief private constructor
////////////////////////////////////////////////////////////////////////////////

  private:
    V8Buffer (v8::Isolate* isolate,
              v8::Handle<v8::Object> wrapper,
              size_t length);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces the buffer
///
/// If replace doesn't have a callback, data must be copied. const_cast in
/// Buffer::New requires this
////////////////////////////////////////////////////////////////////////////////

    void replace (char *data, size_t length, free_callback_fptr callback, void *hint);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief length
////////////////////////////////////////////////////////////////////////////////

    size_t _length;

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer data
////////////////////////////////////////////////////////////////////////////////

    char* _data;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief free callback
////////////////////////////////////////////////////////////////////////////////

    free_callback_fptr _callback;

////////////////////////////////////////////////////////////////////////////////
/// @brief callback hint
////////////////////////////////////////////////////////////////////////////////

    void* _callbackHint;
};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the buffer module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Buffer (v8::Handle<v8::Context> context);

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
