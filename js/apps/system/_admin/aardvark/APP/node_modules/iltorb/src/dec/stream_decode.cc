#include "stream_decode.h"
#include "stream_decode_worker.h"

using namespace v8;

StreamDecode::StreamDecode(Local<Object> params) : next_in(NULL), available_in(0) {
  state = BrotliDecoderCreateInstance(Allocator::Alloc, Allocator::Free, &alloc);
  alloc.ReportMemoryToV8();

  Local<String> key;

  key = Nan::New<String>("dictionary").ToLocalChecked();
  if (Nan::Has(params, key).FromJust()) {
    Local<Object> dictionary = Nan::Get(params, key).ToLocalChecked()->ToObject();
    const size_t dict_size = node::Buffer::Length(dictionary);
    const char* dict_buffer = node::Buffer::Data(dictionary);

    BrotliDecoderSetCustomDictionary(state, dict_size, (const uint8_t*) dict_buffer);
  }
}

StreamDecode::~StreamDecode() {
  BrotliDecoderDestroyInstance(state);
}

void StreamDecode::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("StreamDecode").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "transform", Transform);
  Nan::SetPrototypeMethod(tpl, "flush", Flush);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("StreamDecode").ToLocalChecked(),
    Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(StreamDecode::New) {
  StreamDecode* obj = new StreamDecode(info[0]->ToObject());
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(StreamDecode::Transform) {
  StreamDecode* obj = ObjectWrap::Unwrap<StreamDecode>(info.Holder());

  Local<Object> buffer = info[0]->ToObject();
  obj->next_in = (const uint8_t*) node::Buffer::Data(buffer);
  obj->available_in = node::Buffer::Length(buffer);

  Nan::Callback *callback = new Nan::Callback(info[1].As<Function>());
  StreamDecodeWorker *worker = new StreamDecodeWorker(callback, obj);
  if (info[2]->BooleanValue()) {
    Nan::AsyncQueueWorker(worker);
  } else {
    worker->Execute();
    worker->WorkComplete();
    worker->Destroy();
  }
}

NAN_METHOD(StreamDecode::Flush) {
  StreamDecode* obj = ObjectWrap::Unwrap<StreamDecode>(info.Holder());

  Nan::Callback *callback = new Nan::Callback(info[0].As<Function>());
  obj->next_in = nullptr;
  obj->available_in = 0;
  StreamDecodeWorker *worker = new StreamDecodeWorker(callback, obj);
  if (info[1]->BooleanValue()) {
    Nan::AsyncQueueWorker(worker);
  } else {
    worker->Execute();
    worker->WorkComplete();
    worker->Destroy();
  }
}

Nan::Persistent<Function> StreamDecode::constructor;
