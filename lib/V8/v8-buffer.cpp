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

#include "v8-buffer.h"

#include <v8-profiler.h>

#include "Basics/debugging.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief safety overhead for buffer allocations
////////////////////////////////////////////////////////////////////////////////

#define SAFETY_OVERHEAD 16

static void InitSafetyOverhead(char* p, size_t length) {
  if (p != nullptr) {
    memset(p + length, 0, SAFETY_OVERHEAD);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sliceArgs
////////////////////////////////////////////////////////////////////////////////

static inline bool sliceArgs(v8::Isolate* isolate, v8::Local<v8::Value> const& start_arg,
                             v8::Local<v8::Value> const& end_arg,
                             V8Buffer* parent, int32_t& start, int32_t& end) {
  if (!start_arg->IsInt32() || !end_arg->IsInt32()) {
    TRI_V8_SET_TYPE_ERROR("bad argument");
    return false;
  }
  start = TRI_GET_INT32(start_arg);
  end = TRI_GET_INT32(end_arg);
  if (start < 0 || end < 0) {
    TRI_V8_SET_TYPE_ERROR("bad argument");
    return false;
  }
  if (!(start <= end)) {
    TRI_V8_SET_ERROR("must have start <= end");
    return false;
  }
  if ((size_t)end > parent->_length) {
    TRI_V8_SET_ERROR("end cannot be longer than parent.length");
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief MIN
////////////////////////////////////////////////////////////////////////////////

#define MIN(a, b) ((a) < (b) ? (a) : (b))

////////////////////////////////////////////////////////////////////////////////
/// @brief unbase64
////////////////////////////////////////////////////////////////////////////////

// supports regular and URL-safe base64
static int const unbase64_table[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -2, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 62, -1, 62, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1,
    63, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};

#define unbase64(x) unbase64_table[(uint8_t)(x)]

////////////////////////////////////////////////////////////////////////////////
/// @brief Base64DecodedSize
////////////////////////////////////////////////////////////////////////////////

static size_t Base64DecodedSize(char const* src, size_t size) {
  char const* const end = src + size;
  int const remainder = size % 4;

  size = (size / 4) * 3;
  if (remainder) {
    // special case: 1-byte input cannot be decoded
    if (size == 0 && remainder == 1) {
      size = 0;
    }

    // non-padded input, add 1 or 2 extra bytes
    else {
      size += 1 + (remainder == 3);
    }
  }

  // check for trailing padding (1 or 2 bytes)
  if (size > 0) {
    if (end[-1] == '=') size--;
    if (end[-2] == '=') size--;
  }

  return size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ByteLength
////////////////////////////////////////////////////////////////////////////////

static size_t ByteLengthString(v8::Isolate* isolate, v8::Handle<v8::String> string,
                               TRI_V8_encoding_t enc) {
  v8::HandleScope scope(isolate);

  if (enc == UTF8) {
    return string->Utf8Length(isolate);
  } else if (enc == BASE64) {
    v8::String::Utf8Value v(isolate, string);
    return Base64DecodedSize(*v, v.length());
  } else if (enc == UCS2) {
    return string->Length() * 2;
  } else if (enc == HEX) {
    return string->Length() / 2;
  } else {
    return string->Length();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief encodes a buffer
////////////////////////////////////////////////////////////////////////////////

static void Encode(v8::FunctionCallbackInfo<v8::Value> const& args,
                   const void* buf, size_t len, TRI_V8_encoding_t enc) {
  // cppcheck-suppress *
  v8::Isolate* isolate = args.GetIsolate();

  if (enc == BUFFER) {
    TRI_V8_RETURN(TRI_V8_PAIR_STRING(isolate, static_cast<char const*>(buf), len));
  }

  if (!len) {
    TRI_V8_RETURN_UNDEFINED();
  }

  if (enc == BINARY) {
    const unsigned char* cbuf = static_cast<const unsigned char*>(buf);
    uint16_t* twobytebuf = new uint16_t[len];

    for (size_t i = 0; i < len; i++) {
      // XXX is the following line platform independent?
      twobytebuf[i] = cbuf[i];
    }

    v8::Local<v8::String> chunk = TRI_V8_STRING_UTF16(isolate, twobytebuf, (int)len);
    delete[] twobytebuf;  // TODO use ExternalTwoByteString?

    TRI_V8_RETURN(chunk);
  }

  // utf8 or ascii enc
  v8::Local<v8::String> chunk = TRI_V8_PAIR_STRING(isolate, (char const*)buf, (int)len);
  TRI_V8_RETURN(chunk);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the array size
////////////////////////////////////////////////////////////////////////////////

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor template
////////////////////////////////////////////////////////////////////////////////

static void FromConstructorTemplate(v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> t,
                                    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Local<v8::Value> argv[32];
  size_t argc = args.Length();

  if (argc > ARRAY_SIZE(argv)) {
    argc = ARRAY_SIZE(argv);
  }

  for (size_t i = 0; i < argc; ++i) {
    argv[i] = args[(int)i];
  }

  v8::MaybeLocal<v8::Object> ret =
    t->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>())->NewInstance(TRI_IGETC, (int)argc, argv);

  TRI_V8_RETURN(ret.FromMaybe(v8::Local<v8::Object>()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief non ascii test, slow version
////////////////////////////////////////////////////////////////////////////////

static bool ContainsNonAsciiSlow(char const* buf, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (buf[i] & 0x80) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief non ascii test
////////////////////////////////////////////////////////////////////////////////

static bool ContainsNonAscii(char const* src, size_t len) {
  if (len < 16) {
    return ContainsNonAsciiSlow(src, len);
  }

  const unsigned bytes_per_word = ARANGODB_BITS / (8 * sizeof(char));
  const unsigned align_mask = bytes_per_word - 1;
  const unsigned unaligned = reinterpret_cast<uintptr_t>(src) & align_mask;

  if (unaligned > 0) {
    const unsigned n = bytes_per_word - unaligned;

    if (ContainsNonAsciiSlow(src, n)) {
      return true;
    }

    src += n;
    len -= n;
  }

#if ARANGODB_BITS == 64
  typedef uint64_t word;
  uint64_t const mask = 0x8080808080808080ll;
#else
  typedef uint32_t word;
  const uint32_t mask = 0x80808080l;
#endif

  const word* srcw = reinterpret_cast<const word*>(src);

  for (size_t i = 0, n = len / bytes_per_word; i < n; ++i) {
    if (srcw[i] & mask) return true;
  }

  const unsigned remainder = len & align_mask;

  if (remainder > 0) {
    size_t const offset = len - remainder;

    if (ContainsNonAsciiSlow(src + offset, remainder)) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief strips high bit, slow version
////////////////////////////////////////////////////////////////////////////////

static void ForceAsciiSlow(char const* src, char* dst, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    dst[i] = src[i] & 0x7f;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief strips high bit
////////////////////////////////////////////////////////////////////////////////

static void ForceAscii(char const* src, char* dst, size_t len) {
  if (len < 16) {
    ForceAsciiSlow(src, dst, len);
    return;
  }

  const unsigned bytes_per_word = ARANGODB_BITS / (8 * sizeof(char));
  const unsigned align_mask = bytes_per_word - 1;
  const unsigned src_unalign = reinterpret_cast<uintptr_t>(src) & align_mask;
  const unsigned dst_unalign = reinterpret_cast<uintptr_t>(dst) & align_mask;

  if (src_unalign > 0) {
    if (src_unalign == dst_unalign) {
      const unsigned unalign = bytes_per_word - src_unalign;
      ForceAsciiSlow(src, dst, unalign);
      src += unalign;
      dst += unalign;
      len -= src_unalign;
    } else {
      ForceAsciiSlow(src, dst, len);
      return;
    }
  }

#if ARANGODB_BITS == 64
  typedef uint64_t word;
  uint64_t const mask = ~0x8080808080808080ll;
#else
  typedef uint32_t word;
  const uint32_t mask = ~0x80808080l;
#endif

  const word* srcw = reinterpret_cast<const word*>(src);
  word* dstw = reinterpret_cast<word*>(dst);

  for (size_t i = 0, n = len / bytes_per_word; i < n; ++i) {
    dstw[i] = srcw[i] & mask;
  }

  const unsigned remainder = len & align_mask;

  if (remainder > 0) {
    size_t const offset = len - remainder;
    ForceAsciiSlow(src + offset, dst + offset, remainder);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hex2bin
////////////////////////////////////////////////////////////////////////////////

static unsigned hex2bin(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }

  return static_cast<unsigned>(-1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief DecodeWrite
///
/// Returns number of bytes written.
////////////////////////////////////////////////////////////////////////////////

static ssize_t DecodeWrite(v8::Isolate* isolate, char* buf, size_t buflen,
                           v8::Handle<v8::Value> val, TRI_V8_encoding_t encoding) {
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  // A lot of improvement can be made here. See:
  // http://code.google.com/p/v8/issues/detail?id=270
  // http://groups.google.com/group/v8-dev/browse_thread/thread/dba28a81d9215291/ece2b50a3b4022c
  // http://groups.google.com/group/v8-users/browse_thread/thread/1f83b0ba1f0a611

  if (val->IsArray()) {
    return -1;
  }

  bool is_buffer = V8Buffer::hasInstance(isolate, val);

  if (is_buffer && (encoding == BINARY || encoding == BUFFER)) {
    // fast path, copy buffer data
    char const* data = V8Buffer::data(isolate, val.As<v8::Object>());
    size_t size = V8Buffer::length(isolate, val.As<v8::Object>());
    size_t len = size < buflen ? size : buflen;
    memcpy(buf, data, len);
    return (ssize_t)len;
  }

  v8::Local<v8::String> str;

  // slow path, convert to binary string
  if (is_buffer) {
    v8::Local<v8::Value> arg = TRI_V8_ASCII_STRING(isolate, "binary");
    v8::Handle<v8::Object> object = val.As<v8::Object>();
    v8::Local<v8::Function> callback = TRI_GetProperty(context, isolate, object, "toString").As<v8::Function>();
    str = TRI_GET_STRING(callback->Call(TRI_IGETC, object, 1, &arg).FromMaybe(v8::Local<v8::Value>()));
  } else {
    str = TRI_GET_STRING(val);
  }

  if (encoding == UTF8) {
    str->WriteUtf8(isolate, buf, (int)buflen, NULL, v8::String::HINT_MANY_WRITES_EXPECTED);
    return (ssize_t)buflen;
  }

  if (encoding == ASCII) {
    str->WriteOneByte(isolate, reinterpret_cast<uint8_t*>(buf), 0, (int)buflen,
                      v8::String::HINT_MANY_WRITES_EXPECTED);
    return (ssize_t)buflen;
  }

  // THIS IS AWFUL!!! FIXME
  TRI_ASSERT(encoding == BINARY);

  uint16_t* twobytebuf = new uint16_t[buflen];

  str->Write(isolate, twobytebuf, 0, (int)buflen, v8::String::HINT_MANY_WRITES_EXPECTED);

  for (size_t i = 0; i < buflen; i++) {
    unsigned char* b = reinterpret_cast<unsigned char*>(&twobytebuf[i]);
    buf[i] = b[0];
  }

  delete[] twobytebuf;

  return (ssize_t)buflen;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if we are big endian
////////////////////////////////////////////////////////////////////////////////

static bool IsBigEndian() {
  const union {
    // cppcheck-suppress unusedStructMember
    uint8_t u8[2];
    uint16_t u16;
  } u = {{0, 1}};
  return u.u16 == 1 ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reverses a buffer
////////////////////////////////////////////////////////////////////////////////

static void Swizzle(char* buf, size_t len) {
  for (size_t i = 0; i < len / 2; ++i) {
    char t = buf[i];
    buf[i] = buf[len - i - 1];
    buf[len - i - 1] = t;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an encoding
////////////////////////////////////////////////////////////////////////////////

static TRI_V8_encoding_t ParseEncoding(v8::Isolate* isolate, v8::Handle<v8::Value> encoding_v,
                                       TRI_V8_encoding_t defenc) {
  v8::HandleScope scope(isolate);

  if (!encoding_v->IsString()) {
    return defenc;
  }

  v8::String::Utf8Value encoding(isolate, encoding_v);

  if (strcasecmp(*encoding, "utf8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "utf-8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "ascii") == 0) {
    return ASCII;
  } else if (strcasecmp(*encoding, "base64") == 0) {
    return BASE64;
  } else if (strcasecmp(*encoding, "ucs2") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "ucs-2") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "utf16le") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "utf-16le") == 0) {
    return UCS2;
  } else if (strcasecmp(*encoding, "binary") == 0) {
    return BINARY;
  } else if (strcasecmp(*encoding, "buffer") == 0) {
    return BUFFER;
  } else if (strcasecmp(*encoding, "hex") == 0) {
    return HEX;
  } else {
    return defenc;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new buffer from arguments
////////////////////////////////////////////////////////////////////////////////

void V8Buffer::New(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  if (!args.IsConstructCall()) {
    TRI_GET_GLOBAL(BufferTempl, v8::FunctionTemplate);
    FromConstructorTemplate(isolate, BufferTempl, args);
    return;
  }

  if (!args[0]->IsUint32()) {
    TRI_V8_THROW_TYPE_ERROR("bad argument");
  }

  size_t length = TRI_GET_UINT32(args[0]);

  if (length > kMaxLength) {
    TRI_V8_THROW_RANGE_ERROR("length > kMaxLength");
  }

  new V8Buffer(isolate, args.This(), length);

  TRI_V8_RETURN(args.This());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief C++ API for constructing fast buffer
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> V8Buffer::New(v8::Isolate* isolate, v8::Handle<v8::String> string) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  auto context = TRI_IGETC;

  // get Buffer from global scope.
  v8::Local<v8::Object> global = TRI_IGETC->Global();
  TRI_GET_GLOBAL_STRING(BufferConstant);
  v8::Local<v8::Value> bv = global->Get(context, BufferConstant).FromMaybe(v8::Local<v8::Value>());

  if (!bv->IsFunction()) {
    return v8::Object::New(isolate);
  }

  v8::Local<v8::Function> b = v8::Local<v8::Function>::Cast(bv);

  v8::Local<v8::Value> argv[1] = {v8::Local<v8::Value>::New(isolate, string)};
  v8::Local<v8::Object> instance =
      b->NewInstance(TRI_IGETC, 1, argv).FromMaybe(v8::Local<v8::Object>());

  return instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new buffer with length
////////////////////////////////////////////////////////////////////////////////

V8Buffer* V8Buffer::New(v8::Isolate* isolate, size_t length) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Local<v8::Value> arg = v8::Integer::NewFromUnsigned(isolate, (uint32_t)length);
  TRI_GET_GLOBAL(BufferTempl, v8::FunctionTemplate);
  v8::Local<v8::Object> b =
    BufferTempl->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>())->NewInstance(TRI_IGETC, 1, &arg).FromMaybe(v8::Local<v8::Object>());

  if (b.IsEmpty()) {
    return NULL;
  }

  return V8Buffer::unwrap(b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, data is copied
////////////////////////////////////////////////////////////////////////////////

V8Buffer* V8Buffer::New(v8::Isolate* isolate, char const* data, size_t length) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Local<v8::Value> arg = v8::Integer::NewFromUnsigned(isolate, 0);
  TRI_GET_GLOBAL(BufferTempl, v8::FunctionTemplate);
  v8::Local<v8::Object> obj =
    BufferTempl->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>())
    ->NewInstance(TRI_IGETC, 1, &arg).FromMaybe(v8::Local<v8::Object>());

  V8Buffer* buffer = V8Buffer::unwrap(obj);
  buffer->replace(isolate, const_cast<char*>(data), length, NULL, NULL);

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new buffer from buffer with free callback
////////////////////////////////////////////////////////////////////////////////

V8Buffer* V8Buffer::New(v8::Isolate* isolate, char* data, size_t length,
                        free_callback_fptr callback, void* hint) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Local<v8::Value> arg = v8::Integer::NewFromUnsigned(isolate, 0);
  TRI_GET_GLOBAL(BufferTempl, v8::FunctionTemplate);
  v8::Local<v8::Object> obj =
    BufferTempl->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>())
    ->NewInstance(TRI_IGETC, 1, &arg).FromMaybe(v8::Local<v8::Object>());

  V8Buffer* buffer = V8Buffer::unwrap(obj);
  buffer->replace(isolate, data, length, callback, hint);

  return buffer;
}

V8Buffer::~V8Buffer() { replace(_isolate, NULL, 0, NULL, NULL, true); }

////////////////////////////////////////////////////////////////////////////////
/// @brief private constructor
////////////////////////////////////////////////////////////////////////////////

V8Buffer::V8Buffer(v8::Isolate* isolate, v8::Handle<v8::Object> wrapper, size_t length)
    : V8Wrapper<V8Buffer, TRI_V8_BUFFER_CID>(isolate, this, nullptr, wrapper),  // TODO: warning C4355: 'this' :
                                                                                // used in base member initializer
                                                                                // list
      _length(0),
      _data(nullptr),
      _callback(nullptr) {
  replace(isolate, NULL, length, NULL, NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////

bool V8Buffer::hasInstance(v8::Isolate* isolate, v8::Handle<v8::Value> val) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (!val->IsObject()) {
    return false;
  }

  v8::Local<v8::Object> obj =
      val->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());

  // Also check for SlowBuffers that are empty.
  TRI_GET_GLOBAL(BufferTempl, v8::FunctionTemplate);
  if (BufferTempl->HasInstance(obj)) {
    return true;
  }

  if (TRI_HasProperty(context, isolate, obj, "__buffer__")) {
    return true;
  }

  return strcmp(*v8::String::Utf8Value(isolate, obj->GetConstructorName()),
                "Buffer") == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces the buffer
////////////////////////////////////////////////////////////////////////////////

void V8Buffer::replace(v8::Isolate* isolate, char* data, size_t length,
                       free_callback_fptr callback, void* hint, bool deleteIt) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  if (_callback != nullptr) {
    _callback(_data, _callbackHint);
  } else if (0 < _length) {
    delete[] _data;
    isolate->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<intptr_t>(sizeof(V8Buffer) + _length));
  } else {
    delete[] _data;
  }

  _length = length;
  _callback = callback;
  _callbackHint = hint;

  if (_callback != nullptr) {
    _data = data;
  } else if (0 < _length) {
    _data = new char[_length + SAFETY_OVERHEAD];
    InitSafetyOverhead(_data, _length);

    if (data != nullptr) {
      memcpy(_data, data, _length);
    }

    isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(V8Buffer) + _length + SAFETY_OVERHEAD);
  } else {
    _data = NULL;
  }

  if (!deleteIt) {
    auto handle = v8::Local<v8::Object>::New(isolate, _handle);
    TRI_GET_GLOBAL(LengthKey, v8::String);
    auto len = v8::Integer::NewFromUnsigned(isolate, (uint32_t)_length);
    handle->Set(TRI_IGETC, LengthKey, len).FromMaybe(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief binarySlice
////////////////////////////////////////////////////////////////////////////////

static void JS_BinarySlice(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  int32_t start;
  int32_t end;

  V8Buffer* parent = V8Buffer::unwrap(args.This());

  if (!sliceArgs(isolate, args[0], args[1], parent, start, end)) {
    return;
  }

  char* data = parent->_data + start;
  Encode(args, data, end - start, BINARY);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief asciiSlice
////////////////////////////////////////////////////////////////////////////////

static void JS_AsciiSlice(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  int32_t start;
  int32_t end;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  if (!sliceArgs(isolate, args[0], args[1], parent, start, end)) {
    return;
  }

  char* data = parent->_data + start;
  size_t len = end - start;

  if (ContainsNonAscii(data, len)) {
    char* out = new char[len + SAFETY_OVERHEAD];
    InitSafetyOverhead(out, len);
    ForceAscii(data, out, len);
    v8::Local<v8::String> rc = TRI_V8_PAIR_STRING(isolate, out, (int)len);
    delete[] out;
    TRI_V8_RETURN(rc);
  }

  TRI_V8_RETURN(TRI_V8_PAIR_STRING(isolate, data, (int)len));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief utf8Slice
////////////////////////////////////////////////////////////////////////////////

static void JS_Utf8Slice(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  int32_t start;
  int32_t end;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  if (!sliceArgs(isolate, args[0], args[1], parent, start, end)) {
    return;
  }

  char* data = parent->_data + start;
  TRI_V8_RETURN(TRI_V8_PAIR_STRING(isolate, data, end - start));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Slice
////////////////////////////////////////////////////////////////////////////////

static void JS_Ucs2Slice(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  int32_t start;
  int32_t end;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  if (!sliceArgs(isolate, args[0], args[1], parent, start, end)) {
    return;
  }

  uint16_t* data = (uint16_t*)(parent->_data + start);
  TRI_V8_RETURN(TRI_V8_STRING_UTF16(isolate, data, (end - start) / 2));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hexSlice
////////////////////////////////////////////////////////////////////////////////

static void JS_HexSlice(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  int32_t start;
  int32_t end;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  if (!sliceArgs(isolate, args[0], args[1], parent, start, end)) {
    return;
  }

  char* src = parent->_data + start;
  uint32_t dstlen = (end - start) * 2;

  if (dstlen == 0) {
    TRI_V8_RETURN(v8::String::Empty(isolate));
  }

  char* dst = new char[dstlen + SAFETY_OVERHEAD];
  InitSafetyOverhead(dst, dstlen);

  for (uint32_t i = 0, k = 0; k < dstlen; i += 1, k += 2) {
    static char const hex[] = "0123456789abcdef";
    uint8_t val = static_cast<uint8_t>(src[i]);
    dst[k + 0] = hex[val >> 4];
    dst[k + 1] = hex[val & 15];
  }

  v8::Local<v8::String> string = TRI_V8_PAIR_STRING(isolate, dst, dstlen);
  delete[] dst;
  TRI_V8_RETURN(string);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Slice
////////////////////////////////////////////////////////////////////////////////

static void JS_Base64Slice(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  int32_t start;
  int32_t end;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  if (!sliceArgs(isolate, args[0], args[1], parent, start, end)) {
    return;
  }

  unsigned slen = end - start;
  char const* src = parent->_data + start;

  unsigned dlen = (slen + 2 - ((slen + 2) % 3)) / 3 * 4;
  char* dst = new char[dlen + SAFETY_OVERHEAD];
  InitSafetyOverhead(dst, dlen);

  unsigned a;
  unsigned b;
  unsigned i;
  unsigned k;
  unsigned n;

  static char const table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  i = 0;
  k = 0;
  n = slen / 3 * 3;

  while (i < n) {
    a = src[i + 0] & 0xff;
    b = src[i + 1] & 0xff;
    unsigned c = src[i + 2] & 0xff;

    dst[k + 0] = table[a >> 2];
    dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
    dst[k + 2] = table[((b & 0x0f) << 2) | (c >> 6)];
    dst[k + 3] = table[c & 0x3f];

    i += 3;
    k += 4;
  }

  if (n != slen) {
    switch (slen - n) {
      case 1:
        a = src[i + 0] & 0xff;
        dst[k + 0] = table[a >> 2];
        dst[k + 1] = table[(a & 3) << 4];
        dst[k + 2] = '=';
        dst[k + 3] = '=';
        break;

      case 2:
        a = src[i + 0] & 0xff;
        b = src[i + 1] & 0xff;
        dst[k + 0] = table[a >> 2];
        dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
        dst[k + 2] = table[(b & 0x0f) << 2];
        dst[k + 3] = '=';
        break;
    }
  }

  v8::Local<v8::String> string = TRI_V8_PAIR_STRING(isolate, dst, dlen);
  delete[] dst;
  TRI_V8_RETURN(string);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fill
////////////////////////////////////////////////////////////////////////////////

static void JS_Fill(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  int32_t start;
  int32_t end;

  if (!args[0]->IsInt32()) {
    TRI_V8_THROW_EXCEPTION_USAGE("fill(<char>, <start>, <end>)");
  }

  int value = (char)TRI_GET_INT32(args[0]);

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  if (!sliceArgs(isolate, args[1], args[2], parent, start, end)) {
    return;
  }

  memset((void*)(parent->_data + start), value, end - start);

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy
////////////////////////////////////////////////////////////////////////////////

static void JS_Copy(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8Buffer* source = V8Buffer::unwrap(args.This());

  if (source == nullptr) {
    TRI_V8_THROW_EXCEPTION_USAGE("expecting a buffer as this");
  }

  v8::Local<v8::Value> target = args[0];
  char* target_data = V8Buffer::data(isolate, target);

  if (target_data == nullptr || source == nullptr || source->_data == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid pointer value");
  }

  size_t target_length = V8Buffer::length(isolate, target);
  size_t target_start = TRI_GET_UINT32(args[1]);
  size_t source_start = TRI_GET_UINT32(args[2]);
  size_t source_end =
      args[3]->IsUndefined() ? source->_length : TRI_GET_UINT32(args[3]);

  if (source_end < source_start) {
    TRI_V8_THROW_RANGE_ERROR("sourceEnd < sourceStart");
  }

  // Copy 0 bytes; we're done
  if (source_end == source_start) {
    TRI_V8_RETURN(v8::Integer::New(isolate, 0));
  }

  if (target_start >= target_length) {
    TRI_V8_THROW_RANGE_ERROR("targetStart out of bounds");
  }

  if (source_start >= source->_length) {
    TRI_V8_THROW_RANGE_ERROR("sourceStart out of bounds");
  }

  if (source_end > source->_length) {
    TRI_V8_THROW_RANGE_ERROR("sourceEnd out of bounds");
  }

  size_t to_copy = MIN(MIN(source_end - source_start, target_length - target_start),
                       source->_length - source_start);

  // need to use slightly slower memmove is the ranges might overlap
  memmove((void*)(target_data + target_start),
          (const void*)(source->_data + source_start), to_copy);

  TRI_V8_RETURN(v8::Integer::New(isolate, (int32_t)to_copy));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief utf8Write
////////////////////////////////////////////////////////////////////////////////

static void JS_Utf8Write(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "utf8Write(<string>, <offset>, [<maxLength>])");
  }

  v8::Local<v8::String> s = TRI_GET_STRING(args[0]);

  size_t offset = TRI_GET_UINT32(args[1]);
  int length = s->Length();

  if (length == 0) {
    TRI_V8_RETURN(v8::Integer::New(isolate, 0));
  }

  if (length > 0 && offset >= buffer->_length) {
    TRI_V8_THROW_RANGE_ERROR("<offset> is out of bounds");
  }

  size_t max_length = args[2]->IsUndefined() ? buffer->_length - offset
                                             : TRI_GET_UINT32(args[2]);
  max_length = MIN(buffer->_length - offset, max_length);

  char* p = buffer->_data + offset;

  int char_written;

  int written = s->WriteUtf8(isolate, p, (int)max_length, &char_written,
                             (v8::String::HINT_MANY_WRITES_EXPECTED |
                              v8::String::NO_NULL_TERMINATION));

  TRI_V8_RETURN(v8::Integer::New(isolate, written));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Write
////////////////////////////////////////////////////////////////////////////////

static void JS_Ucs2Write(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("ucs2Write(string, offset, [maxLength])");
  }

  v8::Local<v8::String> s = TRI_GET_STRING(args[0]);
  size_t offset = TRI_GET_UINT32(args[1]);

  if (s->Length() > 0 && offset >= buffer->_length) {
    TRI_V8_THROW_RANGE_ERROR("<offset> is out of bounds");
  }

  size_t max_length = args[2]->IsUndefined() ? buffer->_length - offset
                                             : TRI_GET_UINT32(args[2]);

  max_length = MIN(buffer->_length - offset, max_length) / 2;

  uint16_t* p = (uint16_t*)(buffer->_data + offset);

  int written =
      s->Write(isolate, p, 0, (int)max_length,
               (v8::String::HINT_MANY_WRITES_EXPECTED | v8::String::NO_NULL_TERMINATION));

  TRI_V8_RETURN(v8::Integer::New(isolate, written * 2));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hexWrite
////////////////////////////////////////////////////////////////////////////////

static void JS_HexWrite(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8Buffer* parent = V8Buffer::unwrap(args.This());

  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("hexWrite(string, offset, [maxLength])");
  }

  v8::Local<v8::String> s = args[0].As<v8::String>();

  if (s->Length() % 2 != 0) {
    TRI_V8_THROW_TYPE_ERROR("invalid hex string");
  }

  uint32_t start = TRI_GET_UINT32(args[1]);
  uint32_t size = TRI_GET_UINT32(args[2]);
  uint32_t end = start + size;

  if (start >= parent->_length) {
    TRI_V8_RETURN(v8::Integer::New(isolate, 0));
  }

  // overflow + bounds check.
  if (end < start || end > parent->_length) {
    // dead assignment: end = (uint32_t) parent->_length;
    size = (uint32_t)(parent->_length - start);
  }

  if (size == 0) {
    TRI_V8_RETURN(v8::Integer::New(isolate, 0));
  }

  char* dst = parent->_data + start;
  v8::String::Utf8Value string(isolate, s);
  char const* src = *string;
  uint32_t max = string.length() / 2;

  if (max > size) {
    max = size;
  }

  for (uint32_t i = 0; i < max; ++i) {
    unsigned a = hex2bin(src[i * 2 + 0]);
    unsigned b = hex2bin(src[i * 2 + 1]);

    if (!~a || !~b) {
      TRI_V8_THROW_TYPE_ERROR("invalid hex string");
    }

    dst[i] = a * 16 + b;
  }

  TRI_V8_RETURN(v8::Integer::New(isolate, max));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief asciiWrite
////////////////////////////////////////////////////////////////////////////////

static void JS_AsciiWrite(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "asciiWrite(<string>, <offset>, [<maxLength>])");
  }

  v8::Local<v8::String> s = TRI_GET_STRING(args[0]);
  size_t length = s->Length();
  size_t offset = TRI_GET_UINT32(args[1]);

  if (length > 0 && offset >= buffer->_length) {
    TRI_V8_THROW_TYPE_ERROR("<offset> is out of bounds");
  }

  size_t max_length = args[2]->IsUndefined() ? buffer->_length - offset
                                             : TRI_GET_UINT32(args[2]);

  max_length = MIN(length, MIN(buffer->_length - offset, max_length));

  char* p = buffer->_data + offset;

  int written =
      s->WriteOneByte(isolate, reinterpret_cast<uint8_t*>(p), 0, (int)max_length,
                      (v8::String::HINT_MANY_WRITES_EXPECTED | v8::String::NO_NULL_TERMINATION));
  TRI_V8_RETURN(v8::Integer::New(isolate, written));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Write
////////////////////////////////////////////////////////////////////////////////

static void JS_Base64Write(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "base64Write(<string>, <offset>, [<maxLength>])");
  }

  v8::String::Utf8Value s(isolate, args[0]);
  size_t length = s.length();
  size_t offset = TRI_GET_UINT32(args[1]);
  size_t max_length = args[2]->IsUndefined() ? buffer->_length - offset
                                             : TRI_GET_UINT32(args[2]);

  max_length = MIN(length, MIN(buffer->_length - offset, max_length));

  if (max_length && offset >= buffer->_length) {
    TRI_V8_THROW_TYPE_ERROR("<offset> is out of bounds");
  }

  char* start = buffer->_data + offset;
  char* dst = start;
  char* const dstEnd = dst + max_length;
  char const* src = *s;
  char const* const srcEnd = src + s.length();

  while (src < srcEnd && dst < dstEnd) {
    int remaining = (int)(srcEnd - src);

    while (unbase64(*src) < 0 && src < srcEnd) {
      src++, remaining--;
    }
    if (remaining == 0 || *src == '=') break;
    char a = unbase64(*src++);

    while (unbase64(*src) < 0 && src < srcEnd) {
      src++, remaining--;
    }
    if (remaining <= 1 || *src == '=') break;
    char b = unbase64(*src++);

    *dst++ = (a << 2) | ((b & 0x30) >> 4);
    if (dst == dstEnd) break;

    while (unbase64(*src) < 0 && src < srcEnd) {
      src++, remaining--;
    }
    if (remaining <= 2 || *src == '=') break;
    char c = unbase64(*src++);

    *dst++ = ((b & 0x0F) << 4) | ((c & 0x3C) >> 2);
    if (dst == dstEnd) break;

    while (unbase64(*src) < 0 && src < srcEnd) {
      src++, remaining--;
    }
    if (remaining <= 3 || *src == '=') break;
    char d = unbase64(*src++);

    *dst++ = ((c & 0x03) << 6) | (d & 0x3F);
  }

  TRI_V8_RETURN(v8::Integer::New(isolate, (int32_t)(dst - start)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief binaryWrite
////////////////////////////////////////////////////////////////////////////////

static void JS_BinaryWrite(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "binaryWrite(<string>, <offset>, [<maxLength>])");
  }

  v8::Local<v8::String> s = TRI_GET_STRING(args[0]);
  size_t length = s->Length();
  size_t offset = TRI_GET_UINT32(args[1]);

  if (s->Length() > 0 && offset >= buffer->_length) {
    TRI_V8_THROW_TYPE_ERROR("<offset> is out of bounds");
  }

  char* p = (char*)buffer->_data + offset;
  size_t max_length = args[2]->IsUndefined() ? buffer->_length - offset
                                             : TRI_GET_UINT32(args[2]);

  max_length = MIN(length, MIN(buffer->_length - offset, max_length));

  ssize_t written = DecodeWrite(isolate, p, max_length, s, BINARY);

  TRI_V8_RETURN(v8::Integer::New(isolate, (int32_t)written));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a float (generic version)
////////////////////////////////////////////////////////////////////////////////

template <typename T, bool ENDIANNESS>
static void ReadFloatGeneric(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  double offset_tmp = TRI_GET_DOUBLE(args[0]);
  int64_t offset = static_cast<int64_t>(offset_tmp);
  bool doTRI_ASSERT = !args[1]->BooleanValue(isolate);

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (doTRI_ASSERT) {
    if (offset_tmp != offset || offset < 0) {
      TRI_V8_THROW_TYPE_ERROR("<offset> is not uint");
    }

    size_t len = static_cast<size_t>(buffer->_length);

    if (offset + sizeof(T) > len) {
      TRI_V8_THROW_RANGE_ERROR("trying to read beyond buffer length");
    }
  }

  char* data = static_cast<char*>(buffer->_data);
  char* ptr = data + offset;

  T val;
  memcpy(&val, ptr, sizeof(T));

  if (ENDIANNESS != IsBigEndian()) {
    Swizzle(reinterpret_cast<char*>(&val), sizeof(T));
  }

  TRI_V8_RETURN(v8::Number::New(isolate, val));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  readFloatLE
////////////////////////////////////////////////////////////////////////////////

static void JS_ReadFloatLE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ReadFloatGeneric<float, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief readFloatBE
////////////////////////////////////////////////////////////////////////////////

static void JS_ReadFloatBE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ReadFloatGeneric<float, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief readDoubleLE
////////////////////////////////////////////////////////////////////////////////

static void JS_ReadDoubleLE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  return ReadFloatGeneric<double, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief readDoubleBE
////////////////////////////////////////////////////////////////////////////////

static void JS_ReadDoubleBE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ReadFloatGeneric<double, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a float (generic version)
////////////////////////////////////////////////////////////////////////////////

template <typename T, bool ENDIANNESS>
static void WriteFloatGeneric(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  bool doTRI_ASSERT = !args[2]->BooleanValue(isolate);

  if (doTRI_ASSERT) {
    if (!args[0]->IsNumber()) {
      TRI_V8_THROW_TYPE_ERROR("<value> not a number");
    }

    if (!args[1]->IsUint32()) {
      TRI_V8_THROW_TYPE_ERROR("<offset> is not uint");
    }
  }

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  T val = static_cast<T>(TRI_GET_DOUBLE(args[0]));
  size_t offset = TRI_GET_UINT32(args[1]);
  char* data = static_cast<char*>(buffer->_data);
  char* ptr = data + offset;

  if (doTRI_ASSERT) {
    size_t len = static_cast<size_t>(buffer->_length);

    if (offset + sizeof(T) > len || offset + sizeof(T) < offset) {
      TRI_V8_THROW_RANGE_ERROR("trying to write beyond buffer length");
    }
  }

  memcpy(ptr, &val, sizeof(T));

  if (ENDIANNESS != IsBigEndian()) {
    Swizzle(ptr, sizeof(T));
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeFloatLE
////////////////////////////////////////////////////////////////////////////////

static void JS_WriteFloatLE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  WriteFloatGeneric<float, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeFloatBE
////////////////////////////////////////////////////////////////////////////////

static void JS_WriteFloatBE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  WriteFloatGeneric<float, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeDoubleLE
////////////////////////////////////////////////////////////////////////////////

static void JS_WriteDoubleLE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  WriteFloatGeneric<double, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeDoubleBE
////////////////////////////////////////////////////////////////////////////////

static void JS_WriteDoubleBE(v8::FunctionCallbackInfo<v8::Value> const& args) {
  WriteFloatGeneric<double, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief byteLength
////////////////////////////////////////////////////////////////////////////////

static void JS_ByteLength(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (!args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("byteLength(<string>, <utf8>)");
  }

  v8::Local<v8::String> s = TRI_GET_STRING(args[0]);
  TRI_V8_encoding_t e = ParseEncoding(isolate, args[1], UTF8);

  TRI_V8_RETURN(v8::Integer::New(isolate, (int32_t)ByteLengthString(isolate, s, e)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an indexed attribute from the buffer
////////////////////////////////////////////////////////////////////////////////

static void MapGetIndexedBuffer(uint32_t idx,
                                const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() == 0) {
    // seems object has become a FastBuffer already
    if (TRI_HasProperty(context, isolate, self, "parent")) {
      v8::Handle<v8::Value> parent =
        self->Get(context, TRI_V8_ASCII_STRING(isolate, "parent")).FromMaybe(v8::Handle<v8::Value>());
      if (!parent->IsObject()) {
        TRI_V8_RETURN(v8::Handle<v8::Value>());
      }
      self = TRI_GetObject(context, parent);
      // intentionally falls through
    }
  }

  V8Buffer* buffer = V8Buffer::unwrap(self);

  if (buffer == nullptr || idx >= buffer->_length) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  TRI_V8_RETURN(v8::Integer::NewFromUnsigned(isolate, ((uint8_t)buffer->_data[idx])));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an indexed attribute in the buffer
////////////////////////////////////////////////////////////////////////////////

static void MapSetIndexedBuffer(uint32_t idx, v8::Local<v8::Value> value,
                                const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() == 0) {
    // seems object has become a FastBuffer already
    if (TRI_HasProperty(context, isolate, self, "parent")) {
      v8::Handle<v8::Value> parent =
        self->Get(context, TRI_V8_ASCII_STRING(isolate, "parent")).FromMaybe(v8::Handle<v8::Value>());
      if (!parent->IsObject()) {
        TRI_V8_RETURN(v8::Handle<v8::Value>());
      }
      self = TRI_GetObject(context, parent);
      // intentionally falls through
    }
  }

  V8Buffer* buffer = V8Buffer::unwrap(self);

  if (buffer == nullptr || idx >= buffer->_length) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  auto val = static_cast<uint8_t>(TRI_GET_DOUBLE(value));

  buffer->_data[idx] = (char)val;

  TRI_V8_RETURN(v8::Integer::NewFromUnsigned(isolate, ((uint8_t)buffer->_data[idx])));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the buffer module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Buffer(v8::Isolate* isolate) {
  v8::HandleScope scope(isolate);

  // sanity checks
  TRI_ASSERT(unbase64('/') == 63);
  TRI_ASSERT(unbase64('+') == 62);
  TRI_ASSERT(unbase64('T') == 19);
  TRI_ASSERT(unbase64('Z') == 25);
  TRI_ASSERT(unbase64('t') == 45);
  TRI_ASSERT(unbase64('z') == 51);
  TRI_ASSERT(unbase64(' ') == -2);
  TRI_ASSERT(unbase64('\n') == -2);
  TRI_ASSERT(unbase64('\r') == -2);

  TRI_v8_global_t* v8g = TRI_GetV8Globals(isolate);


  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "BYTELENGTH"), JS_ByteLength);

  // .............................................................................
  // generate the general SlowBuffer template
  // .............................................................................
  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::ObjectTemplate> rt;

  ft = v8::FunctionTemplate::New(isolate, V8Buffer::New);

  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "SlowBuffer"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(1);

  // accessor for indexed properties (e.g. buffer[1])
  rt->SetIndexedPropertyHandler(MapGetIndexedBuffer, MapSetIndexedBuffer);

  v8g->BufferTempl.Reset(isolate, ft);

  // copy free
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "binarySlice"), JS_BinarySlice);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "asciiSlice"), JS_AsciiSlice);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "base64Slice"), JS_Base64Slice);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "ucs2Slice"), JS_Ucs2Slice);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "hexSlice"), JS_HexSlice);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "utf8Slice"), JS_Utf8Slice);

  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "utf8Write"), JS_Utf8Write);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "asciiWrite"), JS_AsciiWrite);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "binaryWrite"), JS_BinaryWrite);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "base64Write"), JS_Base64Write);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "ucs2Write"), JS_Ucs2Write);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "hexWrite"), JS_HexWrite);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "readFloatLE"), JS_ReadFloatLE);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "readFloatBE"), JS_ReadFloatBE);
  TRI_V8_AddProtoMethod(isolate, ft,
                        TRI_V8_ASCII_STRING(isolate, "readDoubleLE"), JS_ReadDoubleLE);
  TRI_V8_AddProtoMethod(isolate, ft,
                        TRI_V8_ASCII_STRING(isolate, "readDoubleBE"), JS_ReadDoubleBE);
  TRI_V8_AddProtoMethod(isolate, ft,
                        TRI_V8_ASCII_STRING(isolate, "writeFloatLE"), JS_WriteFloatLE);
  TRI_V8_AddProtoMethod(isolate, ft,
                        TRI_V8_ASCII_STRING(isolate, "writeFloatBE"), JS_WriteFloatBE);
  TRI_V8_AddProtoMethod(isolate, ft,
                        TRI_V8_ASCII_STRING(isolate, "writeDoubleLE"), JS_WriteDoubleLE);
  TRI_V8_AddProtoMethod(isolate, ft,
                        TRI_V8_ASCII_STRING(isolate, "writeDoubleBE"), JS_WriteDoubleBE);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "fill"), JS_Fill);
  TRI_V8_AddProtoMethod(isolate, ft, TRI_V8_ASCII_STRING(isolate, "copy"), JS_Copy);


  // create the exports
  v8::Handle<v8::ObjectTemplate> exports = v8::ObjectTemplate::New(isolate);


  exports->Set(TRI_V8_ASCII_STRING(isolate, "SlowBuffer"), ft);
  TRI_AddGlobalVariableVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "EXPORTS_SLOW_BUFFER"),
                               exports->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()));
}
