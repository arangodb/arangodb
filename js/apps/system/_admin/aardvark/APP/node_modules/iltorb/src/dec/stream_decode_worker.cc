#include "stream_decode_worker.h"

using namespace v8;

StreamDecodeWorker::StreamDecodeWorker(Nan::Callback *callback, StreamDecode* obj)
  : Nan::AsyncWorker(callback), obj(obj) {}

StreamDecodeWorker::~StreamDecodeWorker() {
}

void StreamDecodeWorker::Execute() {
  do {
    size_t available_out = 0;
    res = BrotliDecoderDecompressStream(obj->state,
                                 &obj->available_in,
                                 &obj->next_in,
                                 &available_out,
                                 NULL,
                                 NULL);

    if (res == BROTLI_DECODER_RESULT_ERROR) {
      return;
    }

    while (BrotliDecoderHasMoreOutput(obj->state) == BROTLI_TRUE) {
      size_t size = 0;
      const uint8_t* output = BrotliDecoderTakeOutput(obj->state, &size);

      void* buf = obj->alloc.Alloc(size);
      if (!buf) {
        res = BROTLI_DECODER_RESULT_ERROR;
        return;
      }

      memcpy(buf, output, size);
      obj->pending_output.push_back(static_cast<uint8_t*>(buf));
    }
  } while(res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);
}

void StreamDecodeWorker::HandleOKCallback() {
  if (res == BROTLI_DECODER_RESULT_ERROR || res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
    Local<Value> argv[] = {
      Nan::Error("Brotli failed to decompress.")
    };
    callback->Call(1, argv);
  } else {
    Local<Value> argv[] = {
      Nan::Null(),
      obj->PendingChunksAsArray()
    };
    callback->Call(2, argv);
  }

  obj->alloc.ReportMemoryToV8();
}
