#include "stream_encode.h"
#include "stream_encode_worker.h"

using namespace v8;

StreamEncode::StreamEncode(Local<Object> params) {
  state = BrotliEncoderCreateInstance(Allocator::Alloc, Allocator::Free, &alloc);

  Local<String> key;
  uint32_t val;

  key = Nan::New<String>("mode").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    val = Nan::Get(params, key).ToLocalChecked()->Int32Value();
    BrotliEncoderSetParameter(state, BROTLI_PARAM_MODE, val);
  }

  key = Nan::New<String>("quality").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    val = Nan::Get(params, key).ToLocalChecked()->Int32Value();
    BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, val);
  }

  key = Nan::New<String>("lgwin").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    val = Nan::Get(params, key).ToLocalChecked()->Int32Value();
    BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, val);
  }

  key = Nan::New<String>("lgblock").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    val = Nan::Get(params, key).ToLocalChecked()->Int32Value();
    BrotliEncoderSetParameter(state, BROTLI_PARAM_LGBLOCK, val);
  }

  key = Nan::New<String>("size_hint").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    val = Nan::Get(params, key).ToLocalChecked()->BooleanValue();
    BrotliEncoderSetParameter(state, BROTLI_PARAM_SIZE_HINT, val);
  }

  key = Nan::New<String>("disable_literal_context_modeling").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    val = Nan::Get(params, key).ToLocalChecked()->Int32Value();
    BrotliEncoderSetParameter(state, BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING, val);
  }

  key = Nan::New<String>("dictionary").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    Local<Object> dictionary = Nan::Get(params, key).ToLocalChecked()->ToObject();
    const size_t dict_size = node::Buffer::Length(dictionary);
    const char* dict_buffer = node::Buffer::Data(dictionary);

    BrotliEncoderSetCustomDictionary(state, dict_size, (const uint8_t*) dict_buffer);
  }
}

StreamEncode::~StreamEncode() {
  BrotliEncoderDestroyInstance(state);
}

void StreamEncode::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("StreamEncode").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "transform", Transform);
  Nan::SetPrototypeMethod(tpl, "flush", Flush);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("StreamEncode").ToLocalChecked(),
    Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(StreamEncode::New) {
  StreamEncode* obj = new StreamEncode(info[0]->ToObject());
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(StreamEncode::Transform) {
  StreamEncode* obj = ObjectWrap::Unwrap<StreamEncode>(info.Holder());

  Local<Object> buffer = info[0]->ToObject();
  obj->next_in = (const uint8_t*) node::Buffer::Data(buffer);
  obj->available_in = node::Buffer::Length(buffer);

  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());
  StreamEncodeWorker *worker = new StreamEncodeWorker(callback, obj, BROTLI_OPERATION_PROCESS);
  if (info[2]->BooleanValue()) {
    Nan::AsyncQueueWorker(worker);
  } else {
    worker->Execute();
    worker->WorkComplete();
    worker->Destroy();
  }
}

NAN_METHOD(StreamEncode::Flush) {
  StreamEncode* obj = ObjectWrap::Unwrap<StreamEncode>(info.Holder());

  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());
  BrotliEncoderOperation op = info[0]->BooleanValue()
    ? BROTLI_OPERATION_FINISH
    : BROTLI_OPERATION_FLUSH;
  obj->next_in = nullptr;
  obj->available_in = 0;
  StreamEncodeWorker *worker = new StreamEncodeWorker(callback, obj, op);
  if (info[2]->BooleanValue()) {
    Nan::AsyncQueueWorker(worker);
  } else {
    worker->Execute();
    worker->WorkComplete();
    worker->Destroy();
  }
}

Nan::Persistent<Function> StreamEncode::constructor;
