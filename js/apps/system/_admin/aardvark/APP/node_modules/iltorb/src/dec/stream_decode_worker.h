#ifndef STREAM_DECODE_WORKER_H
#define STREAM_DECODE_WORKER_H

#include <nan.h>
#include "stream_decode.h"
#include "brotli/decode.h"

class StreamDecodeWorker : public Nan::AsyncWorker {
  public:
    StreamDecodeWorker(Nan::Callback *callback, StreamDecode* obj);

    void Execute();
    void HandleOKCallback();

  private:
    ~StreamDecodeWorker();
    StreamDecode* obj;
    BrotliDecoderResult res;
};

#endif
