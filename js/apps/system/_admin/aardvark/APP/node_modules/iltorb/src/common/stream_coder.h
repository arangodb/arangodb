#ifndef STREAM_CODER_H
#define STREAM_CODER_H

#include "allocator.h"
#include <nan.h>

using namespace v8;

class StreamCoder : public Nan::ObjectWrap {
  public:
    Allocator alloc;
    std::vector<uint8_t*> pending_output;

    Local<Array> PendingChunksAsArray();
  protected:
    explicit StreamCoder();
    ~StreamCoder();
};

#endif
