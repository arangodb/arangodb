#ifndef STREAM_DECODE_H
#define STREAM_DECODE_H

#include <nan.h>
#include "../common/stream_coder.h"
#include "brotli/decode.h"

class StreamDecode : public StreamCoder {
  public:
    static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target);

    const uint8_t* next_in;
    size_t available_in;

    BrotliDecoderState* state;
  private:
    explicit StreamDecode(Local<Object> params);
    ~StreamDecode();

    static NAN_METHOD(New);
    static NAN_METHOD(Transform);
    static NAN_METHOD(Flush);
    static Nan::Persistent<v8::Function> constructor;
};

#endif
