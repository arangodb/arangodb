#include <nan.h>
#include "dec/stream_decode.h"
#include "enc/stream_encode.h"

using namespace v8;

NAN_MODULE_INIT(Init) {
  StreamDecode::Init(target);
  StreamEncode::Init(target);
}

NODE_MODULE(iltorb, Init)
