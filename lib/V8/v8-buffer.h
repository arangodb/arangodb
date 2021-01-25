////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
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

#ifndef ARANGODB_V8_V8__BUFFER_H
#define ARANGODB_V8_V8__BUFFER_H 1

#include "Basics/Common.h"

#include "V8/v8-globals.h"
#include "V8/v8-wrapper.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer encoding
////////////////////////////////////////////////////////////////////////////////

typedef enum TRI_V8_encoding_e {
  ASCII,
  UTF8,
  BASE64,
  UCS2,
  BINARY,
  HEX,
  BUFFER
} TRI_V8_encoding_t;

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
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximal length
  ///
  /// mirrors deps/v8/src/objects.h
  //////////////////////////////////////////////////////////////////////////////

  static const unsigned int kMaxLength = 0x3fffffff;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if the object is an instance
  //////////////////////////////////////////////////////////////////////////////

  static bool hasInstance(v8::Isolate* isolate, v8::Handle<v8::Value> val);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the buffer data for a handle
  //////////////////////////////////////////////////////////////////////////////

  static inline char* data(v8::Isolate* iso, v8::Handle<v8::Value> val) {
    TRI_ASSERT(val->IsObject());
    v8::Local<v8::Context> context = iso->GetCurrentContext();
    auto o = TRI_GetObject(context, val);
    int32_t offsetValue = 0;

    if (o->InternalFieldCount() == 0) {
      // seems object has become a FastBuffer already
      ISOLATE;

      if (TRI_HasProperty(context, isolate, o, "offset")) {
        v8::Handle<v8::Value> offset = TRI_GetProperty(context, isolate, o, "offset");
        if (offset->IsNumber()) {
          offsetValue = TRI_GET_INT32(offset);
        }
      }

      if (TRI_HasProperty(context, isolate, o, "parent")) {
        v8::Handle<v8::Value> parent = TRI_GetProperty(context, isolate, o, "parent");
        if (!parent->IsObject()) {
          return nullptr;
        }
        o = TRI_ToObject(context, parent);
        // intentionally falls through
      }
    }

    V8Buffer* buffer = unwrap(o);
    if (buffer == nullptr || offsetValue < 0) {
      return nullptr;
    }

    size_t length = buffer->_length;
    if (static_cast<size_t>(offsetValue) >= length) {
      return nullptr;  // OOB
    }

    return buffer->_data + offsetValue;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the buffer data
  //////////////////////////////////////////////////////////////////////////////

  static inline char* data(v8::Isolate* isolate, V8Buffer* b) {
    auto val = v8::Local<v8::Object>::New(isolate, b->_handle);
    return data(isolate, val);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief length of the data for a handle
  //////////////////////////////////////////////////////////////////////////////

  static inline size_t length(v8::Isolate* iso, v8::Handle<v8::Value> val) {
    TRI_ASSERT(val->IsObject());
    v8::Local<v8::Context> context = iso->GetCurrentContext();
    auto o = TRI_GetObject(context, val);
    int32_t lengthValue = -1;

    if (o->InternalFieldCount() == 0) {
      // seems object has become a FastBuffer already
      ISOLATE;

      if (TRI_HasProperty(context, isolate, o, "length")) {
        v8::Handle<v8::Value> length = TRI_GetProperty(context, isolate, o, "length");
        if (length->IsNumber()) {
          lengthValue = TRI_GET_INT32(length);
        }
      }

      if (TRI_HasProperty(context, isolate, o, "parent")) {
        v8::Handle<v8::Value> parent = TRI_GetProperty(context, isolate, o, "parent");
        if (!parent->IsObject()) {
          return 0;
        }
        o = TRI_ToObject(context, parent);
        // intentionally falls through
      }
    }

    V8Buffer* buffer = unwrap(o);
    if (buffer == nullptr) {
      return 0;
    }

    if (lengthValue >= 0) {
      return static_cast<size_t>(lengthValue);
    }

    return buffer->_length;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief length of the data
  //////////////////////////////////////////////////////////////////////////////

  static inline size_t length(v8::Isolate* isolate, V8Buffer* b) {
    auto val = v8::Local<v8::Object>::New(isolate, b->_handle);
    return length(isolate, val);
  }

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief free callback type
  //////////////////////////////////////////////////////////////////////////////

  typedef void (*free_callback_fptr)(char* data, void* hint);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief instance constructor
  //////////////////////////////////////////////////////////////////////////////

  static void New(v8::FunctionCallbackInfo<v8::Value> const& args);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief C++ API for constructing fast buffer
  //////////////////////////////////////////////////////////////////////////////

  static v8::Handle<v8::Object> New(v8::Isolate* isolate, v8::Handle<v8::String> string);

  static V8Buffer* New(v8::Isolate* isolate, size_t length);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor, data is copied
  //////////////////////////////////////////////////////////////////////////////

  static V8Buffer* New(v8::Isolate* isolate, char const* data, size_t length);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor with free callback
  //////////////////////////////////////////////////////////////////////////////

  static V8Buffer* New(v8::Isolate* isolate, char* data, size_t length,
                       free_callback_fptr callback, void* hint);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief desctructor
  //////////////////////////////////////////////////////////////////////////////

  ~V8Buffer();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief private constructor
  //////////////////////////////////////////////////////////////////////////////

 private:
  V8Buffer(v8::Isolate* isolate, v8::Handle<v8::Object> wrapper, size_t length);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief replaces the buffer
  ///
  /// If replace doesn't have a callback, data must be copied. const_cast in
  /// Buffer::New requires this
  //////////////////////////////////////////////////////////////////////////////

  void replace(v8::Isolate* isolate, char* data, size_t length,
               free_callback_fptr callback, void* hint, bool deleteIt = false);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief length
  //////////////////////////////////////////////////////////////////////////////

  size_t _length;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief buffer data
  //////////////////////////////////////////////////////////////////////////////

  char* _data;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief free callback
  //////////////////////////////////////////////////////////////////////////////

  free_callback_fptr _callback;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief callback hint
  //////////////////////////////////////////////////////////////////////////////

  void* _callbackHint;
};

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the buffer module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Buffer(v8::Isolate* isolate);
