#include <nan.h>
#include "stream_decode.h"

using namespace v8;

NAN_MODULE_INIT(Init) {
  StreamDecode::Init(target);
}

NODE_MODULE(decode, Init)
