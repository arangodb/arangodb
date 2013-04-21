////////////////////////////////////////////////////////////////////////////////
/// @brief binary buffer
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triAGENS GmbH, Cologne, Germany
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

#include "v8-buffer.h"

#include <v8-profiler.h>

#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief SLICE_ARGS
////////////////////////////////////////////////////////////////////////////////

#define SLICE_ARGS(scope, start_arg, end_arg)                        \
  if (!start_arg->IsInt32() || !end_arg->IsInt32()) {                \
    TRI_V8_TYPE_ERROR(scope, "bad argument");                        \
  }                                                                  \
  int32_t start = start_arg->Int32Value();                           \
  int32_t end = end_arg->Int32Value();                               \
  if (start < 0 || end < 0) {                                        \
    TRI_V8_TYPE_ERROR(scope, "bad argument");                        \
  }                                                                  \
  if (!(start <= end)) {                                             \
    TRI_V8_ERROR(scope, "must have start <= end");                   \
  }                                                                  \
  if ((size_t)end > parent->_length) {                               \
    TRI_V8_ERROR(scope, "end cannot be longer than parent.length");  \
  }                                                                  \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief MIN
////////////////////////////////////////////////////////////////////////////////

#define MIN(a,b) ((a) < (b) ? (a) : (b))

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief unbase64
////////////////////////////////////////////////////////////////////////////////

// supports regular and URL-safe base64
static const int unbase64_table[] =
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-1,-1,-2,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,62,-1,63
  ,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1
  ,-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14
  ,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63
  ,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40
  ,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };

#define unbase64(x) unbase64_table[(uint8_t)(x)]

////////////////////////////////////////////////////////////////////////////////
/// @brief Base64DecodedSize
////////////////////////////////////////////////////////////////////////////////

static size_t Base64DecodedSize (const char *src, size_t size) {
  const char *const end = src + size;
  const int remainder = size % 4;

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

static size_t ByteLengthString (v8::Handle<v8::String> string,
                                TRI_V8_encoding_t enc) {
  v8::HandleScope scope;

  if (enc == UTF8) {
    return string->Utf8Length();
  }
  else if (enc == BASE64) {
    v8::String::Utf8Value v(string);
    return Base64DecodedSize(*v, v.length());
  }
  else if (enc == UCS2) {
    return string->Length() * 2;
  }
  else if (enc == HEX) {
    return string->Length() / 2;
  }
  else {
    return string->Length();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief encodes a buffer
////////////////////////////////////////////////////////////////////////////////

static v8::Local<v8::Value> Encode (const void *buf,
                                    size_t len,
                                    TRI_V8_encoding_t enc) {
  v8::HandleScope scope;

  if (enc == BUFFER) {
    return scope.Close(
        V8Buffer::New(static_cast<const char*>(buf), len)->_handle);
  }

  if (!len) {
    return scope.Close(v8::String::Empty());
  }

  if (enc == BINARY) {
    const unsigned char *cbuf = static_cast<const unsigned char*>(buf);
    uint16_t * twobytebuf = new uint16_t[len];

    for (size_t i = 0; i < len; i++) {
      // XXX is the following line platform independent?
      twobytebuf[i] = cbuf[i];
    }

    v8::Local<v8::String> chunk = v8::String::New(twobytebuf, len);
    delete [] twobytebuf; // TODO use ExternalTwoByteString?

    return scope.Close(chunk);
  }

  // utf8 or ascii enc
  v8::Local<v8::String> chunk = v8::String::New((const char*) buf, len);
  return scope.Close(chunk);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the array size
////////////////////////////////////////////////////////////////////////////////

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor template
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> FromConstructorTemplate (v8::Persistent<v8::FunctionTemplate> t,
                                               const v8::Arguments& args) {
  v8::HandleScope scope;

  v8::Local<v8::Value> argv[32];
  size_t argc = args.Length();

  if (argc > ARRAY_SIZE(argv)) {
    argc = ARRAY_SIZE(argv);
  }

  for (size_t i = 0;  i < argc;  ++i) {
    argv[i] = args[i];
  }

  return scope.Close(t->GetFunction()->NewInstance(argc, argv));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief non ascii test, slow version
////////////////////////////////////////////////////////////////////////////////

static bool ContainsNonAsciiSlow (const char* buf, size_t len) {
  for (size_t i = 0;  i < len;  ++i) {
    if (buf[i] & 0x80) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief non ascii test
////////////////////////////////////////////////////////////////////////////////

static bool ContainsNonAscii (const char* src, size_t len) {
  if (len < 16) {
    return ContainsNonAsciiSlow(src, len);
  }

  const unsigned bytes_per_word = TRI_BITS / (8 * sizeof(char));
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

#if TRI_BITS == 54
  typedef uint64_t word;
  const uint64_t mask = 0x8080808080808080ll;
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
    const size_t offset = len - remainder;

    if (ContainsNonAsciiSlow(src + offset, remainder)) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief strips high bit, slow version
////////////////////////////////////////////////////////////////////////////////

static void ForceAsciiSlow (const char* src, char* dst, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    dst[i] = src[i] & 0x7f;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief strips high bit
////////////////////////////////////////////////////////////////////////////////

static void ForceAscii (const char* src, char* dst, size_t len) {
  if (len < 16) {
    ForceAsciiSlow(src, dst, len);
    return;
  }

  const unsigned bytes_per_word = TRI_BITS / (8 * sizeof(char));
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
    }
    else {
      ForceAsciiSlow(src, dst, len);
      return;
    }
  }

#if TRI_BITS == 64
  typedef uint64_t word;
  const uint64_t mask = ~0x8080808080808080ll;
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
    const size_t offset = len - remainder;
    ForceAsciiSlow(src + offset, dst + offset, remainder);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hex2bin
////////////////////////////////////////////////////////////////////////////////

static unsigned hex2bin (char c) {
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'A' && c <= 'F') { return 10 + (c - 'A'); }
  if (c >= 'a' && c <= 'f') { return 10 + (c - 'a'); }

  return static_cast<unsigned>(-1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief DecodeWrite
///
/// Returns number of bytes written.
////////////////////////////////////////////////////////////////////////////////

static ssize_t DecodeWrite (char *buf,
                            size_t buflen,
                            v8::Handle<v8::Value> val,
                            TRI_V8_encoding_t encoding) {
  v8::HandleScope scope;

  // A lot of improvement can be made here. See:
  // http://code.google.com/p/v8/issues/detail?id=270
  // http://groups.google.com/group/v8-dev/browse_thread/thread/dba28a81d9215291/ece2b50a3b4022c
  // http://groups.google.com/group/v8-users/browse_thread/thread/1f83b0ba1f0a611

  if (val->IsArray()) {
    return -1;
  }

  bool is_buffer = V8Buffer::hasInstance(val);

  if (is_buffer && (encoding == BINARY || encoding == BUFFER)) {
    // fast path, copy buffer data
    const char* data = V8Buffer::data(val.As<v8::Object>());
    size_t size = V8Buffer::length(val.As<v8::Object>());
    size_t len = size < buflen ? size : buflen;
    memcpy(buf, data, len);
    return len;
  }

  v8::Local<v8::String> str;

  // slow path, convert to binary string
  if (is_buffer) {
    v8::Local<v8::Value> arg = v8::String::New("binary");
    v8::Handle<v8::Object> object = val.As<v8::Object>();
    v8::Local<v8::Function> callback = object->Get(TRI_V8_SYMBOL("toString")).As<v8::Function>();
    str = callback->Call(object, 1, &arg)->ToString();
  }
  else {
    str = val->ToString();
  }

  if (encoding == UTF8) {
    str->WriteUtf8(buf, buflen, NULL, v8::String::HINT_MANY_WRITES_EXPECTED);
    return buflen;
  }

  if (encoding == ASCII) {
    str->WriteOneByte(reinterpret_cast<uint8_t*>(buf),
                      0,
                      buflen,
                      v8::String::HINT_MANY_WRITES_EXPECTED);
    return buflen;
  }

  // THIS IS AWFUL!!! FIXME
  assert(encoding == BINARY);

  uint16_t * twobytebuf = new uint16_t[buflen];

  str->Write(twobytebuf, 0, buflen, v8::String::HINT_MANY_WRITES_EXPECTED);

  for (size_t i = 0; i < buflen; i++) {
    unsigned char *b = reinterpret_cast<unsigned char*>(&twobytebuf[i]);
    buf[i] = b[0];
  }

  delete [] twobytebuf;

  return buflen;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if we are big endian
////////////////////////////////////////////////////////////////////////////////

static bool IsBigEndian () {
  const union { uint8_t u8[2]; uint16_t u16; } u = {{0, 1}};
  return u.u16 == 1 ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reverses a buffer
////////////////////////////////////////////////////////////////////////////////

static void Swizzle (char* buf, size_t len) {
  char t;

  for (size_t i = 0;  i < len / 2;  ++i) {
    t = buf[i];
    buf[i] = buf[len - i - 1];
    buf[len - i - 1] = t;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an encoding
////////////////////////////////////////////////////////////////////////////////

static TRI_V8_encoding_t ParseEncoding (v8::Handle<v8::Value> encoding_v,
                                        TRI_V8_encoding_t defenc) {
  v8::HandleScope scope;

  if (! encoding_v->IsString()) {
    return defenc;
  }

  v8::String::Utf8Value encoding(encoding_v);

  if (strcasecmp(*encoding, "utf8") == 0) {
    return UTF8;
  }
  else if (strcasecmp(*encoding, "utf-8") == 0) {
    return UTF8;
  }
  else if (strcasecmp(*encoding, "ascii") == 0) {
    return ASCII;
  }
  else if (strcasecmp(*encoding, "base64") == 0) {
    return BASE64;
  }
  else if (strcasecmp(*encoding, "ucs2") == 0) {
    return UCS2;
  }
  else if (strcasecmp(*encoding, "ucs-2") == 0) {
    return UCS2;
  }
  else if (strcasecmp(*encoding, "utf16le") == 0) {
    return UCS2;
  }
  else if (strcasecmp(*encoding, "utf-16le") == 0) {
    return UCS2;
  }
  else if (strcasecmp(*encoding, "binary") == 0) {
    return BINARY;
  }
  else if (strcasecmp(*encoding, "buffer") == 0) {
    return BUFFER;
  }
  else if (strcasecmp(*encoding, "hex") == 0) {
    return HEX;
  }
  else {
    return defenc;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                          class RetainedBufferInfo
// -----------------------------------------------------------------------------

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief object info
////////////////////////////////////////////////////////////////////////////////

  class RetainedBufferInfo : public v8::RetainedObjectInfo {
    public:
      RetainedBufferInfo (V8Buffer* buffer);

    public:
      virtual void Dispose ();
      virtual bool IsEquivalent (RetainedObjectInfo* other);
      virtual intptr_t GetHash ();
      virtual const char* GetLabel ();
      virtual intptr_t GetSizeInBytes ();

    private:
      static const char _label[];

    private:
      V8Buffer* _buffer;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                          static private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the class
////////////////////////////////////////////////////////////////////////////////

  const char RetainedBufferInfo::_label[] = "Buffer";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

  RetainedBufferInfo::RetainedBufferInfo (V8Buffer* buffer)
    : _buffer(buffer) {
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new wrapper info
////////////////////////////////////////////////////////////////////////////////

  v8::RetainedObjectInfo* WrapperInfo (uint16_t classId, v8::Handle<v8::Value> wrapper) {
    assert(classId == TRI_V8_BUFFER_CID);
    assert(V8Buffer::hasInstance(wrapper));

    V8Buffer* buffer = V8Buffer::unwrap(wrapper.As<v8::Object>());
    return new RetainedBufferInfo(buffer);
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the info
////////////////////////////////////////////////////////////////////////////////

  void RetainedBufferInfo::Dispose () {
    _buffer = NULL;
    delete this;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for equivalence
////////////////////////////////////////////////////////////////////////////////

  bool RetainedBufferInfo::IsEquivalent (RetainedObjectInfo* other) {
    return _label == other->GetLabel() 
      &&  _buffer == static_cast<RetainedBufferInfo*>(other)->_buffer;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the equivalence hash
////////////////////////////////////////////////////////////////////////////////

  intptr_t RetainedBufferInfo::GetHash () {
    return reinterpret_cast<intptr_t>(_buffer);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the label of the class
////////////////////////////////////////////////////////////////////////////////

  const char* RetainedBufferInfo::GetLabel () {
    return _label;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the size in bytes
////////////////////////////////////////////////////////////////////////////////

  intptr_t RetainedBufferInfo::GetSizeInBytes () {
    return V8Buffer::length(_buffer);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  classes V8Buffer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new buffer from arguments
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8Buffer::New (const v8::Arguments& args) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  if (! args.IsConstructCall()) {
    return scope.Close(FromConstructorTemplate(v8g->BufferTempl, args));
  }

  if (! args[0]->IsUint32()) {
    TRI_V8_TYPE_ERROR(scope, "bad argument");
  }

  size_t length = args[0]->Uint32Value();

  if (length > kMaxLength) {
    TRI_V8_RANGE_ERROR(scope, "length > kMaxLength");
  }

  new V8Buffer(isolate, args.This(), length);

  return scope.Close(args.This());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief C++ API for constructing fast buffer
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> V8Buffer::New (v8::Handle<v8::String> string) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  // get Buffer from global scope.
  v8::Local<v8::Object> global = v8::Context::GetCurrent()->Global();
  v8::Local<v8::Value> bv = global->Get(v8g->BufferConstant);

  if (! bv->IsFunction()) {
    return scope.Close(v8::Object::New());
  }

  v8::Local<v8::Function> b = v8::Local<v8::Function>::Cast(bv);

  v8::Local<v8::Value> argv[1] = { v8::Local<v8::Value>::New(string) };
  v8::Local<v8::Object> instance = b->NewInstance(1, argv);

  return scope.Close(instance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new buffer with length
////////////////////////////////////////////////////////////////////////////////

V8Buffer* V8Buffer::New (size_t length) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Local<v8::Value> arg = v8::Integer::NewFromUnsigned(length, isolate);
  v8::Local<v8::Object> b = v8g->BufferTempl->GetFunction()->NewInstance(1, &arg);

  if (b.IsEmpty()) {
    return NULL;
  }

  return V8Buffer::unwrap(b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, data is copied
////////////////////////////////////////////////////////////////////////////////

V8Buffer* V8Buffer::New (const char* data, size_t length) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Local<v8::Value> arg = v8::Integer::NewFromUnsigned(0, isolate);
  v8::Local<v8::Object> obj = v8g->BufferTempl->GetFunction()->NewInstance(1, &arg);

  V8Buffer* buffer = V8Buffer::unwrap(obj);
  buffer->replace(const_cast<char*>(data), length, NULL, NULL);

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new buffer from buffer with free callback
////////////////////////////////////////////////////////////////////////////////

V8Buffer* V8Buffer::New (char* data,
                         size_t length,
                         free_callback_fptr callback,
                         void* hint) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  v8::Local<v8::Value> arg = v8::Integer::NewFromUnsigned(0, isolate);
  v8::Local<v8::Object> obj = v8g->BufferTempl->GetFunction()->NewInstance(1, &arg);

  V8Buffer *buffer = V8Buffer::unwrap(obj);
  buffer->replace(data, length, callback, hint);

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

V8Buffer::~V8Buffer() {
  replace(NULL, 0, NULL, NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief private constructor
////////////////////////////////////////////////////////////////////////////////

V8Buffer::V8Buffer (v8::Isolate* isolate,
                    v8::Handle<v8::Object> wrapper,
                    size_t length)
  : V8Wrapper<V8Buffer, TRI_V8_BUFFER_CID>(isolate, this, 0, wrapper),
    _length(0),
    _callback(0) {
  replace(NULL, length, NULL, NULL);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief 
////////////////////////////////////////////////////////////////////////////////

bool V8Buffer::hasInstance (v8::Handle<v8::Value> val) {
  TRI_V8_CURRENT_GLOBALS;

  if (! val->IsObject()) {
    return false;
  }

  v8::Local<v8::Object> obj = val->ToObject();
  v8::ExternalArrayType type = obj->GetIndexedPropertiesExternalArrayDataType();

  if (type != v8::kExternalUnsignedByteArray) {
    return false;
  }

  // Also check for SlowBuffers that are empty.
  if (v8g->BufferTempl->HasInstance(obj)) {
    return true;
  }

  assert(! v8g->FastBufferConstructor.IsEmpty());
  return obj->GetConstructor()->StrictEquals(v8g->FastBufferConstructor);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces the buffer
////////////////////////////////////////////////////////////////////////////////

void V8Buffer::replace (char* data,
                        size_t length,
                        free_callback_fptr callback,
                        void* hint) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  if (_callback != 0) {
    _callback(_data, _callbackHint);
  }
  else if (0 < _length) {
    delete [] _data;
    /* isolate-> */ v8::V8::AdjustAmountOfExternalAllocatedMemory(
      -static_cast<intptr_t>(sizeof(V8Buffer) + _length));
  }

  _length = length;
  _callback = callback;
  _callbackHint = hint;

  if (_callback != 0) {
    _data = data;
  }
  else if (0 < _length) {
    _data = new char[_length];

    if (data != 0) {
      memcpy(_data, data, _length);
    }

    /* isolate-> */ v8::V8::AdjustAmountOfExternalAllocatedMemory(
      sizeof(V8Buffer) + _length);
  }
  else {
    _data = NULL;
  }

  _handle->SetIndexedPropertiesToExternalArrayData(_data,
                                                   v8::kExternalUnsignedByteArray,
                                                   _length);

  _handle->Set(v8g->LengthKey, v8::Integer::NewFromUnsigned(_length, isolate));
}

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief binarySlice
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_BinarySlice (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  SLICE_ARGS(scope, args[0], args[1]);

  char* data = parent->_data + start;
  v8::Local<v8::Value> b = Encode(data, end - start, BINARY);

  return scope.Close(b);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief asciiSlice
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AsciiSlice (const v8::Arguments& args) {
  v8::HandleScope scope;
  V8Buffer* parent = V8Buffer::unwrap(args.This());
  SLICE_ARGS(scope, args[0], args[1]);

  char* data = parent->_data + start;
  size_t len = end - start;

  if (ContainsNonAscii(data, len)) {
    char* out = new char[len];
    ForceAscii(data, out, len);
    v8::Local<v8::String> rc = v8::String::New(out, len);
    delete[] out;
    return scope.Close(rc);
  }

  return scope.Close(v8::String::New(data, len));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief utf8Slice
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Utf8Slice (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  SLICE_ARGS(scope, args[0], args[1]);

  char* data = parent->_data + start;
  v8::Local<v8::String> string = v8::String::New(data, end - start);
  return scope.Close(string);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Slice
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Ucs2Slice (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  SLICE_ARGS(scope, args[0], args[1]);

  uint16_t* data = (uint16_t*)(parent->_data + start);
  v8::Local<v8::String> string = v8::String::New(data, (end - start) / 2);
  return scope.Close(string);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hexSlice
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HexSlice (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  SLICE_ARGS(scope, args[0], args[1]);

  char* src = parent->_data + start;
  uint32_t dstlen = (end - start) * 2;

  if (dstlen == 0) {
    return scope.Close(v8::String::Empty());
  }

  char* dst = new char[dstlen];

  for (uint32_t i = 0, k = 0; k < dstlen; i += 1, k += 2) {
    static const char hex[] = "0123456789abcdef";
    uint8_t val = static_cast<uint8_t>(src[i]);
    dst[k + 0] = hex[val >> 4];
    dst[k + 1] = hex[val & 15];
  }

  v8::Local<v8::String> string = v8::String::New(dst, dstlen);
  delete[] dst;
  return scope.Close(string);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Slice
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Base64Slice (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  SLICE_ARGS(scope, args[0], args[1]);

  unsigned slen = end - start;
  const char* src = parent->_data + start;

  unsigned dlen = (slen + 2 - ((slen + 2) % 3)) / 3 * 4;
  char* dst = new char[dlen];

  unsigned a;
  unsigned b;
  unsigned c;
  unsigned i;
  unsigned k;
  unsigned n;

  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz"
                              "0123456789+/";

  i = 0;
  k = 0;
  n = slen / 3 * 3;

  while (i < n) {
    a = src[i + 0] & 0xff;
    b = src[i + 1] & 0xff;
    c = src[i + 2] & 0xff;

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

  v8::Local<v8::String> string = v8::String::New(dst, dlen);
  delete [] dst;

  return scope.Close(string);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fill
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Fill (const v8::Arguments& args) {
  v8::HandleScope scope;

  if (!args[0]->IsInt32()) {
    TRI_V8_EXCEPTION_USAGE(scope, "fill(<char>, <start>, <end>)");
  }

  int value = (char)args[0]->Int32Value();

  V8Buffer* parent = V8Buffer::unwrap(args.This());
  SLICE_ARGS(scope, args[1], args[2]);

  memset( (void*)(parent->_data + start),
          value,
          end - start);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Copy (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* source = V8Buffer::unwrap(args.This());

  if (! V8Buffer::hasInstance(args[0])) {
    TRI_V8_EXCEPTION_USAGE(scope, "copy(<buffer>, [<start>], [<end>])");
  }

  v8::Local<v8::Value> target = args[0];
  char* target_data = V8Buffer::data(target);
  size_t target_length = V8Buffer::length(target);
  size_t target_start = args[1]->IsUndefined() ? 0 : args[1]->Uint32Value();
  size_t source_start = args[2]->IsUndefined() ? 0 : args[2]->Uint32Value();
  size_t source_end = args[3]->IsUndefined() 
                      ? source->_length
                      : args[3]->Uint32Value();

  if (source_end < source_start) {
    TRI_V8_RANGE_ERROR(scope, "sourceEnd < sourceStart");
  }

  // Copy 0 bytes; we're done
  if (source_end == source_start) {
    return scope.Close(v8::Integer::New(0));
  }

  if (target_start >= target_length) {
    TRI_V8_RANGE_ERROR(scope, "targetStart out of bounds");
  }

  if (source_start >= source->_length) {
    TRI_V8_RANGE_ERROR(scope, "sourceStart out of bounds");
  }

  if (source_end > source->_length) {
    TRI_V8_RANGE_ERROR(scope, "sourceEnd out of bounds");
  }

  size_t to_copy = MIN(MIN(source_end - source_start,
                           target_length - target_start),
                           source->_length - source_start);

  // need to use slightly slower memmove is the ranges might overlap
  memmove((void *)(target_data + target_start),
          (const void*)(source->_data + source_start),
          to_copy);

  return scope.Close(v8::Integer::New(to_copy));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief utf8Write
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Utf8Write(const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (!args[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "utf8Write(<string>, <offset>, [<maxLength>])");
  }

  v8::Local<v8::String> s = args[0]->ToString();

  size_t offset = args[1]->Uint32Value();
  int length = s->Length();

  if (length == 0) {
    return scope.Close(v8::Integer::New(0));
  }

  if (length > 0 && offset >= buffer->_length) {
    TRI_V8_RANGE_ERROR(scope, "<offset> is out of bounds");
  }

  size_t max_length = args[2]->IsUndefined() ? buffer->_length - offset
                                             : args[2]->Uint32Value();
  max_length = MIN(buffer->_length - offset, max_length);

  char* p = buffer->_data + offset;

  int char_written;

  int written = s->WriteUtf8(p,
                             max_length,
                             &char_written,
                             (v8::String::HINT_MANY_WRITES_EXPECTED
                            | v8::String::NO_NULL_TERMINATION));

  return scope.Close(v8::Integer::New(written));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ucs2Write
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Ucs2Write (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer *buffer = V8Buffer::unwrap(args.This());

  if (! args[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "ucs2Write(string, offset, [maxLength])");
  }

  v8::Local<v8::String> s = args[0]->ToString();
  size_t offset = args[1]->Uint32Value();

  if (s->Length() > 0 && offset >= buffer->_length) {
    TRI_V8_RANGE_ERROR(scope, "<offset> is out of bounds");
  }

  size_t max_length = args[2]->IsUndefined()
    ? buffer->_length - offset
    : args[2]->Uint32Value();

  max_length = MIN(buffer->_length - offset, max_length) / 2;

  uint16_t* p = (uint16_t*)(buffer->_data + offset);

  int written = s->Write(p,
                         0,
                         max_length,
                         (v8::String::HINT_MANY_WRITES_EXPECTED
                        | v8::String::NO_NULL_TERMINATION));

  return scope.Close(v8::Integer::New(written * 2));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hexWrite
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HexWrite (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* parent = V8Buffer::unwrap(args.This());

  if (! args[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "hexWrite(string, offset, [maxLength])");
  }

  v8::Local<v8::String> s = args[0].As<v8::String>();

  if (s->Length() % 2 != 0) {
    TRI_V8_TYPE_ERROR(scope, "invalid hex string");
  }

  uint32_t start = args[1]->Uint32Value();
  uint32_t size = args[2]->Uint32Value();
  uint32_t end = start + size;

  if (start >= parent->_length) {
    v8::Local<v8::Integer> val = v8::Integer::New(0);
    return scope.Close(val);
  }

  // overflow + bounds check.
  if (end < start || end > parent->_length) {
    end = parent->_length;
    size = parent->_length - start;
  }

  if (size == 0) {
    v8::Local<v8::Integer> val = v8::Integer::New(0);
    return scope.Close(val);
  }

  char* dst = parent->_data + start;
  v8::String::AsciiValue string(s);
  const char* src = *string;
  uint32_t max = string.length() / 2;

  if (max > size) {
    max = size;
  }

  for (uint32_t i = 0; i < max; ++i) {
    unsigned a = hex2bin(src[i * 2 + 0]);
    unsigned b = hex2bin(src[i * 2 + 1]);

    if (!~a || !~b) {
      TRI_V8_TYPE_ERROR(scope, "invalid hex string");
    }

    dst[i] = a * 16 + b;
  }

  return scope.Close(v8::Integer::New(max));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief asciiWrite
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AsciiWrite (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (! args[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "asciiWrite(<string>, <offset>, [<maxLength>])");
  }

  v8::Local<v8::String> s = args[0]->ToString();
  size_t length = s->Length();
  size_t offset = args[1]->Int32Value();

  if (length > 0 && offset >= buffer->_length) {
    TRI_V8_TYPE_ERROR(scope, "<offset> is out of bounds");
  }

  size_t max_length = args[2]->IsUndefined()
    ? buffer->_length - offset
  : args[2]->Uint32Value();

  max_length = MIN(length, MIN(buffer->_length - offset, max_length));

  char* p = buffer->_data + offset;

  int written = s->WriteOneByte(reinterpret_cast<uint8_t*>(p),
                                0,
                                max_length,
                                (v8::String::HINT_MANY_WRITES_EXPECTED
                               | v8::String::NO_NULL_TERMINATION));

  return scope.Close(v8::Integer::New(written));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Write
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Base64Write (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer* buffer = V8Buffer::unwrap(args.This());

  if (! args[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "base64Write(<string>, <offset>, [<maxLength>])");
  }

  v8::String::AsciiValue s(args[0]);
  size_t length = s.length();
  size_t offset = args[1]->Int32Value();
  size_t max_length = args[2]->IsUndefined()
    ? buffer->_length - offset
    : args[2]->Uint32Value();

  max_length = MIN(length, MIN(buffer->_length - offset, max_length));

  if (max_length && offset >= buffer->_length) {
    TRI_V8_TYPE_ERROR(scope, "<offset> is out of bounds");
  }

  char a, b, c, d;
  char* start = buffer->_data + offset;
  char* dst = start;
  char* const dstEnd = dst + max_length;
  const char* src = *s;
  const char* const srcEnd = src + s.length();

  while (src < srcEnd && dst < dstEnd) {
    int remaining = srcEnd - src;

    while (unbase64(*src) < 0 && src < srcEnd) { src++, remaining--; }
    if (remaining == 0 || *src == '=') break;
    a = unbase64(*src++);

    while (unbase64(*src) < 0 && src < srcEnd) { src++, remaining--; }
    if (remaining <= 1 || *src == '=') break;
    b = unbase64(*src++);

    *dst++ = (a << 2) | ((b & 0x30) >> 4);
    if (dst == dstEnd) break;

    while (unbase64(*src) < 0 && src < srcEnd) { src++, remaining--; }
    if (remaining <= 2 || *src == '=') break;
    c = unbase64(*src++);

    *dst++ = ((b & 0x0F) << 4) | ((c & 0x3C) >> 2);
    if (dst == dstEnd) break;

    while (unbase64(*src) < 0 && src < srcEnd) { src++, remaining--; }
    if (remaining <= 3 || *src == '=') break;
    d = unbase64(*src++);

    *dst++ = ((c & 0x03) << 6) | (d & 0x3F);
  }

  return scope.Close(v8::Integer::New(dst - start));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief binaryWrite
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_BinaryWrite (const v8::Arguments& args) {
  v8::HandleScope scope;

  V8Buffer *buffer = V8Buffer::unwrap(args.This());

  if (! args[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "binaryWrite(<string>, <offset>, [<maxLength>])");
  }

  v8::Local<v8::String> s = args[0]->ToString();
  size_t length = s->Length();
  size_t offset = args[1]->Int32Value();

  if (s->Length() > 0 && offset >= buffer->_length) {
    TRI_V8_TYPE_ERROR(scope, "<offset> is out of bounds");
  }

  char *p = (char*)buffer->_data + offset;
  size_t max_length = args[2]->IsUndefined()
    ? buffer->_length - offset
  : args[2]->Uint32Value();

  max_length = MIN(length, MIN(buffer->_length - offset, max_length));

  int written = DecodeWrite(p, max_length, s, BINARY);

  return scope.Close(v8::Integer::New(written));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a float (generic version)
////////////////////////////////////////////////////////////////////////////////

template <typename T, bool ENDIANNESS>
static v8::Handle<v8::Value> ReadFloatGeneric (const v8::Arguments& args) {
  v8::HandleScope scope;

  double offset_tmp = args[0]->NumberValue();
  int64_t offset = static_cast<int64_t>(offset_tmp);
  bool doAssert = !args[1]->BooleanValue();

  if (doAssert) {
    if (offset_tmp != offset || offset < 0) {
      TRI_V8_TYPE_ERROR(scope, "<offset> is not uint");
    }

    size_t len = static_cast<size_t>(
      args.This()->GetIndexedPropertiesExternalArrayDataLength());

    if (offset + sizeof(T) > len) {
      TRI_V8_RANGE_ERROR(scope, "trying to read beyond buffer length");
    }
  }

  char* data = static_cast<char*>(
    args.This()->GetIndexedPropertiesExternalArrayData());
  char* ptr = data + offset;

  T val;
  memcpy(&val, ptr, sizeof(T));

  if (ENDIANNESS != IsBigEndian()) {
    Swizzle(reinterpret_cast<char*>(&val), sizeof(T));
  }

  return v8::Number::New(val);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  readFloatLE
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReadFloatLE (const v8::Arguments& args) {
  return ReadFloatGeneric<float, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief readFloatBE 
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReadFloatBE (const v8::Arguments& args) {
  return ReadFloatGeneric<float, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief readDoubleLE
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReadDoubleLE (const v8::Arguments& args) {
  return ReadFloatGeneric<double, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief readDoubleBE 
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReadDoubleBE (const v8::Arguments& args) {
  return ReadFloatGeneric<double, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a float (generic version) 
////////////////////////////////////////////////////////////////////////////////

template <typename T, bool ENDIANNESS>
static v8::Handle<v8::Value> WriteFloatGeneric (const v8::Arguments& args) {
  v8::HandleScope scope;

  bool doAssert = !args[2]->BooleanValue();

  if (doAssert) {
    if (!args[0]->IsNumber()) {
      TRI_V8_TYPE_ERROR(scope, "<value> not a number");
    }

    if (!args[1]->IsUint32()) {
      TRI_V8_TYPE_ERROR(scope, "<offset> is not uint");
    }
  }

  T val = static_cast<T>(args[0]->NumberValue());
  size_t offset = args[1]->Uint32Value();
  char* data = static_cast<char*>(
    args.This()->GetIndexedPropertiesExternalArrayData());
  char* ptr = data + offset;

  if (doAssert) {
    size_t len = static_cast<size_t>(
      args.This()->GetIndexedPropertiesExternalArrayDataLength());

    if (offset + sizeof(T) > len || offset + sizeof(T) < offset) {
      TRI_V8_RANGE_ERROR(scope, "trying to write beyond buffer length");
    }
  }

  memcpy(ptr, &val, sizeof(T));

  if (ENDIANNESS != IsBigEndian()) {
    Swizzle(ptr, sizeof(T));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeFloatLE
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WriteFloatLE (const v8::Arguments& args) {
  return WriteFloatGeneric<float, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeFloatBE
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WriteFloatBE (const v8::Arguments& args) {
  return WriteFloatGeneric<float, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeDoubleLE
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WriteDoubleLE (const v8::Arguments& args) {
  return WriteFloatGeneric<double, false>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writeDoubleBE
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WriteDoubleBE (const v8::Arguments& args) {
  return WriteFloatGeneric<double, true>(args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief byteLength
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByteLength (const v8::Arguments &args) {
  v8::HandleScope scope;

  if (! args[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "byteLength(<string>, <utf8>)");
  }

  v8::Local<v8::String> s = args[0]->ToString();
  TRI_V8_encoding_t e = ParseEncoding(args[1], UTF8);

  return scope.Close(v8::Integer::New(ByteLengthString(s, e)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief makeFastBuffer
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_MakeFastBuffer (const v8::Arguments &args) {
  v8::HandleScope scope;

  if (! V8Buffer::hasInstance(args[0])) {
    TRI_V8_EXCEPTION_USAGE(scope, "makeFastBuffer(<buffer>, <fastBuffer>, <offset>, <length>");
  }

  V8Buffer *buffer = V8Buffer::unwrap(args[0]->ToObject());
  v8::Local<v8::Object> fast_buffer = args[1]->ToObject();;
  uint32_t offset = args[2]->Uint32Value();
  uint32_t length = args[3]->Uint32Value();

  if (offset > buffer->_length) {
    TRI_V8_RANGE_ERROR(scope, "<offset> out of range");
  }

  if (offset + length > buffer->_length) {
    TRI_V8_RANGE_ERROR(scope, "<length> out of range");
  }

  // Check for wraparound. Safe because offset and length are unsigned.
  if (offset + length < offset) {
    TRI_V8_RANGE_ERROR(scope, "<offset> or <length> out of range");
  }

  fast_buffer->SetIndexedPropertiesToExternalArrayData(
    buffer->_data + offset,
    v8::kExternalUnsignedByteArray,
    length);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief setFastBufferConstructor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SetFastBufferConstructor (const v8::Arguments& args) {
  TRI_V8_CURRENT_GLOBALS_AND_SCOPE;

  if (args[0]->IsFunction()) {
    v8g->FastBufferConstructor = v8::Persistent<v8::Function>::New(
      isolate,
      args[0].As<v8::Function>());
  }

  return scope.Close(v8::Undefined());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  global functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the buffer module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Buffer (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  // sanity checks
  assert(unbase64('/') == 63);
  assert(unbase64('+') == 62);
  assert(unbase64('T') == 19);
  assert(unbase64('Z') == 25);
  assert(unbase64('t') == 45);
  assert(unbase64('z') == 51);
  assert(unbase64(' ') == -2);
  assert(unbase64('\n') == -2);
  assert(unbase64('\r') == -2);

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = TRI_CreateV8Globals(isolate);

  // create the exports
  v8::Handle<v8::Object> exports = v8::Object::New();
  TRI_AddGlobalVariableVocbase(context, "EXPORTS_BUFFER", exports);

  // .............................................................................
  // generate the general SlowBuffer template
  // .............................................................................

  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(V8Buffer::New);
  v8g->BufferTempl = v8::Persistent<v8::FunctionTemplate>::New(isolate, t);
  v8g->BufferTempl->InstanceTemplate()->SetInternalFieldCount(1);
  v8g->BufferTempl->SetClassName(TRI_V8_SYMBOL("SlowBuffer"));

  // copy free
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "binarySlice", JS_BinarySlice);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "asciiSlice", JS_AsciiSlice);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "base64Slice", JS_Base64Slice);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "ucs2Slice", JS_Ucs2Slice);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "hexSlice", JS_HexSlice);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "utf8Slice", JS_Utf8Slice);

  TRI_V8_AddProtoMethod(v8g->BufferTempl, "utf8Write", JS_Utf8Write);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "asciiWrite", JS_AsciiWrite);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "binaryWrite", JS_BinaryWrite);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "base64Write", JS_Base64Write);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "ucs2Write", JS_Ucs2Write);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "hexWrite", JS_HexWrite);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "readFloatLE", JS_ReadFloatLE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "readFloatBE", JS_ReadFloatBE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "readDoubleLE", JS_ReadDoubleLE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "readDoubleBE", JS_ReadDoubleBE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "writeFloatLE", JS_WriteFloatLE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "writeFloatBE", JS_WriteFloatBE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "writeDoubleLE", JS_WriteDoubleLE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "writeDoubleBE", JS_WriteDoubleBE);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "fill", JS_Fill);
  TRI_V8_AddProtoMethod(v8g->BufferTempl, "copy", JS_Copy);

  TRI_V8_AddMethod(v8g->BufferTempl, "byteLength", JS_ByteLength);
  TRI_V8_AddMethod(v8g->BufferTempl, "makeFastBuffer", JS_MakeFastBuffer);

  TRI_V8_AddMethod(exports, "SlowBuffer", v8g->BufferTempl);
  TRI_V8_AddMethod(exports, "setFastBufferConstructor", JS_SetFastBufferConstructor);

  v8::HeapProfiler::DefineWrapperClass(TRI_V8_BUFFER_CID, WrapperInfo);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
