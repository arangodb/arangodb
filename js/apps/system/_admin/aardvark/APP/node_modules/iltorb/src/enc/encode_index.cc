#include <nan.h>
#include "stream_encode.h"

using namespace v8;

NAN_MODULE_INIT(Init) {
  StreamEncode::Init(target);
}

NODE_MODULE(encode, Init)
