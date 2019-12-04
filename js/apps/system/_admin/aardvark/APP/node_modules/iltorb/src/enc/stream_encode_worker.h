#ifndef STREAM_ENCODE_WORKER_H
#define STREAM_ENCODE_WORKER_H

#include <nan.h>
#include "stream_encode.h"
#include "brotli/encode.h"

class StreamEncodeWorker : public Nan::AsyncWorker {
  public:
    StreamEncodeWorker(Nan::Callback *callback, StreamEncode* obj, BrotliEncoderOperation op);

    void Execute();
    void HandleOKCallback();

  private:
    ~StreamEncodeWorker();
    StreamEncode* obj;
    BrotliEncoderOperation op;
    bool res;
};

#endif
