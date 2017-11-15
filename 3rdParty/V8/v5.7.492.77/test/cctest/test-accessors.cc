// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "src/v8.h"

#include "src/api.h"
#include "src/frames-inl.h"
#include "src/string-stream.h"
#include "test/cctest/cctest.h"

using ::v8::ObjectTemplate;
using ::v8::Value;
using ::v8::Context;
using ::v8::Local;
using ::v8::Name;
using ::v8::String;
using ::v8::Script;
using ::v8::Function;
using ::v8::Extension;

static void handle_property(Local<String> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(v8_num(900));
}

static void handle_property_2(Local<String> name,
                              const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(v8_num(902));
}


static void handle_property(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  CHECK_EQ(0, info.Length());
  info.GetReturnValue().Set(v8_num(907));
}


THREADED_TEST(PropertyHandler) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  Local<v8::FunctionTemplate> fun_templ = v8::FunctionTemplate::New(isolate);
  fun_templ->InstanceTemplate()->SetAccessor(v8_str("foo"), handle_property);
  Local<v8::FunctionTemplate> getter_templ =
      v8::FunctionTemplate::New(isolate, handle_property);
  getter_templ->SetLength(0);
  fun_templ->
      InstanceTemplate()->SetAccessorProperty(v8_str("bar"), getter_templ);
  fun_templ->InstanceTemplate()->
      SetNativeDataProperty(v8_str("instance_foo"), handle_property);
  fun_templ->SetNativeDataProperty(v8_str("object_foo"), handle_property_2);
  Local<Function> fun = fun_templ->GetFunction(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("Fun"), fun).FromJust());
  Local<Script> getter;
  Local<Script> setter;
  // check function instance accessors
  getter = v8_compile("var obj = new Fun(); obj.instance_foo;");
  CHECK_EQ(900, getter->Run(env.local())
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
  setter = v8_compile("obj.instance_foo = 901;");
  CHECK_EQ(901, setter->Run(env.local())
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
  getter = v8_compile("obj.bar;");
  CHECK_EQ(907, getter->Run(env.local())
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
  setter = v8_compile("obj.bar = 908;");
  CHECK_EQ(908, setter->Run(env.local())
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
  // check function static accessors
  getter = v8_compile("Fun.object_foo;");
  CHECK_EQ(902, getter->Run(env.local())
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
  setter = v8_compile("Fun.object_foo = 903;");
  CHECK_EQ(903, setter->Run(env.local())
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
}


static void GetIntValue(Local<String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  int* value =
      static_cast<int*>(v8::Local<v8::External>::Cast(info.Data())->Value());
  info.GetReturnValue().Set(v8_num(*value));
}


static void SetIntValue(Local<String> property,
                        Local<Value> value,
                        const v8::PropertyCallbackInfo<void>& info) {
  int* field =
      static_cast<int*>(v8::Local<v8::External>::Cast(info.Data())->Value());
  *field = value->Int32Value(info.GetIsolate()->GetCurrentContext()).FromJust();
}

int foo, bar, baz;

THREADED_TEST(GlobalVariableAccess) {
  foo = 0;
  bar = -4;
  baz = 10;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
  templ->InstanceTemplate()->SetAccessor(
      v8_str("foo"), GetIntValue, SetIntValue,
      v8::External::New(isolate, &foo));
  templ->InstanceTemplate()->SetAccessor(
      v8_str("bar"), GetIntValue, SetIntValue,
      v8::External::New(isolate, &bar));
  templ->InstanceTemplate()->SetAccessor(
      v8_str("baz"), GetIntValue, SetIntValue,
      v8::External::New(isolate, &baz));
  LocalContext env(0, templ->InstanceTemplate());
  v8_compile("foo = (++bar) + baz")->Run(env.local()).ToLocalChecked();
  CHECK_EQ(bar, -3);
  CHECK_EQ(foo, 7);
}


static int x_register[2] = {0, 0};
static v8::Local<v8::Object> x_receiver;
static v8::Local<v8::Object> x_holder;

template<class Info>
static void XGetter(const Info& info, int offset) {
  ApiTestFuzzer::Fuzz();
  v8::Isolate* isolate = CcTest::isolate();
  CHECK_EQ(isolate, info.GetIsolate());
  CHECK(
      x_receiver->Equals(isolate->GetCurrentContext(), info.This()).FromJust());
  info.GetReturnValue().Set(v8_num(x_register[offset]));
}


static void XGetter(Local<String> name,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(x_holder->Equals(info.GetIsolate()->GetCurrentContext(), info.Holder())
            .FromJust());
  XGetter(info, 0);
}


static void XGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(
      x_receiver->Equals(info.GetIsolate()->GetCurrentContext(), info.Holder())
          .FromJust());
  XGetter(info, 1);
}


template<class Info>
static void XSetter(Local<Value> value, const Info& info, int offset) {
  v8::Isolate* isolate = CcTest::isolate();
  CHECK_EQ(isolate, info.GetIsolate());
  CHECK(x_holder->Equals(info.GetIsolate()->GetCurrentContext(), info.This())
            .FromJust());
  CHECK(x_holder->Equals(info.GetIsolate()->GetCurrentContext(), info.Holder())
            .FromJust());
  x_register[offset] =
      value->Int32Value(info.GetIsolate()->GetCurrentContext()).FromJust();
  info.GetReturnValue().Set(v8_num(-1));
}


static void XSetter(Local<String> name,
                    Local<Value> value,
                    const v8::PropertyCallbackInfo<void>& info) {
  XSetter(value, info, 0);
}


static void XSetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK_EQ(1, info.Length());
  XSetter(info[0], info, 1);
}


THREADED_TEST(AccessorIC) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("x0"), XGetter, XSetter);
  obj->SetAccessorProperty(v8_str("x1"),
                           v8::FunctionTemplate::New(isolate, XGetter),
                           v8::FunctionTemplate::New(isolate, XSetter));
  x_holder = obj->NewInstance(context.local()).ToLocalChecked();
  CHECK(context->Global()
            ->Set(context.local(), v8_str("holder"), x_holder)
            .FromJust());
  x_receiver = v8::Object::New(isolate);
  CHECK(context->Global()
            ->Set(context.local(), v8_str("obj"), x_receiver)
            .FromJust());
  v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(
      CompileRun("obj.__proto__ = holder;"
                 "var result = [];"
                 "var key_0 = 'x0';"
                 "var key_1 = 'x1';"
                 "for (var j = 0; j < 10; j++) {"
                 "  var i = 4*j;"
                 "  result.push(holder.x0 = i);"
                 "  result.push(obj.x0);"
                 "  result.push(holder.x1 = i + 1);"
                 "  result.push(obj.x1);"
                 "  result.push(holder[key_0] = i + 2);"
                 "  result.push(obj[key_0]);"
                 "  result.push(holder[key_1] = i + 3);"
                 "  result.push(obj[key_1]);"
                 "}"
                 "result"));
  CHECK_EQ(80u, array->Length());
  for (int i = 0; i < 80; i++) {
    v8::Local<Value> entry =
        array->Get(context.local(), v8::Integer::New(isolate, i))
            .ToLocalChecked();
    CHECK(v8::Integer::New(isolate, i / 2)
              ->Equals(context.local(), entry)
              .FromJust());
  }
}


template <int C>
static void HandleAllocatingGetter(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  for (int i = 0; i < C; i++) {
    v8::String::NewFromUtf8(info.GetIsolate(), "foo",
                            v8::NewStringType::kNormal)
        .ToLocalChecked();
  }
  info.GetReturnValue().Set(v8::String::NewFromUtf8(info.GetIsolate(), "foo",
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked());
}


THREADED_TEST(HandleScopePop) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("one"), HandleAllocatingGetter<1>);
  obj->SetAccessor(v8_str("many"), HandleAllocatingGetter<1024>);
  v8::Local<v8::Object> inst =
      obj->NewInstance(context.local()).ToLocalChecked();
  CHECK(
      context->Global()->Set(context.local(), v8_str("obj"), inst).FromJust());
  int count_before =
      i::HandleScope::NumberOfHandles(reinterpret_cast<i::Isolate*>(isolate));
  {
    v8::HandleScope scope(isolate);
    CompileRun(
        "for (var i = 0; i < 1000; i++) {"
        "  obj.one;"
        "  obj.many;"
        "}");
  }
  int count_after =
      i::HandleScope::NumberOfHandles(reinterpret_cast<i::Isolate*>(isolate));
  CHECK_EQ(count_before, count_after);
}

static void CheckAccessorArgsCorrect(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(info.GetIsolate() == CcTest::isolate());
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()
            ->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("data"))
            .FromJust());
  ApiTestFuzzer::Fuzz();
  CHECK(info.GetIsolate() == CcTest::isolate());
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()
            ->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("data"))
            .FromJust());
  CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
  CHECK(info.GetIsolate() == CcTest::isolate());
  CHECK(info.This() == info.Holder());
  CHECK(info.Data()
            ->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("data"))
            .FromJust());
  info.GetReturnValue().Set(17);
}


THREADED_TEST(DirectCall) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("xxx"), CheckAccessorArgsCorrect, NULL,
                   v8_str("data"));
  v8::Local<v8::Object> inst =
      obj->NewInstance(context.local()).ToLocalChecked();
  CHECK(
      context->Global()->Set(context.local(), v8_str("obj"), inst).FromJust());
  Local<Script> scr =
      v8::Script::Compile(context.local(), v8_str("obj.xxx")).ToLocalChecked();
  for (int i = 0; i < 10; i++) {
    Local<Value> result = scr->Run(context.local()).ToLocalChecked();
    CHECK(!result.IsEmpty());
    CHECK_EQ(17, result->Int32Value(context.local()).FromJust());
  }
}

static void EmptyGetter(Local<String> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  CheckAccessorArgsCorrect(name, info);
  ApiTestFuzzer::Fuzz();
  CheckAccessorArgsCorrect(name, info);
  info.GetReturnValue().Set(v8::Local<v8::Value>());
}


THREADED_TEST(EmptyResult) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("xxx"), EmptyGetter, NULL, v8_str("data"));
  v8::Local<v8::Object> inst =
      obj->NewInstance(context.local()).ToLocalChecked();
  CHECK(
      context->Global()->Set(context.local(), v8_str("obj"), inst).FromJust());
  Local<Script> scr =
      v8::Script::Compile(context.local(), v8_str("obj.xxx")).ToLocalChecked();
  for (int i = 0; i < 10; i++) {
    Local<Value> result = scr->Run(context.local()).ToLocalChecked();
    CHECK(result == v8::Undefined(isolate));
  }
}


THREADED_TEST(NoReuseRegress) {
  // Check that the IC generated for the one test doesn't get reused
  // for the other.
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  {
    v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
    obj->SetAccessor(v8_str("xxx"), EmptyGetter, NULL, v8_str("data"));
    LocalContext context;
    v8::Local<v8::Object> inst =
        obj->NewInstance(context.local()).ToLocalChecked();
    CHECK(context->Global()
              ->Set(context.local(), v8_str("obj"), inst)
              .FromJust());
    Local<Script> scr = v8::Script::Compile(context.local(), v8_str("obj.xxx"))
                            .ToLocalChecked();
    for (int i = 0; i < 2; i++) {
      Local<Value> result = scr->Run(context.local()).ToLocalChecked();
      CHECK(result == v8::Undefined(isolate));
    }
  }
  {
    v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
    obj->SetAccessor(v8_str("xxx"), CheckAccessorArgsCorrect, NULL,
                     v8_str("data"));
    LocalContext context;
    v8::Local<v8::Object> inst =
        obj->NewInstance(context.local()).ToLocalChecked();
    CHECK(context->Global()
              ->Set(context.local(), v8_str("obj"), inst)
              .FromJust());
    Local<Script> scr = v8::Script::Compile(context.local(), v8_str("obj.xxx"))
                            .ToLocalChecked();
    for (int i = 0; i < 10; i++) {
      Local<Value> result = scr->Run(context.local()).ToLocalChecked();
      CHECK(!result.IsEmpty());
      CHECK_EQ(17, result->Int32Value(context.local()).FromJust());
    }
  }
}

static void ThrowingGetAccessor(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetIsolate()->ThrowException(v8_str("g"));
}


static void ThrowingSetAccessor(Local<String> name,
                                Local<Value> value,
                                const v8::PropertyCallbackInfo<void>& info) {
  info.GetIsolate()->ThrowException(value);
}


THREADED_TEST(Regress1054726) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("x"),
                   ThrowingGetAccessor,
                   ThrowingSetAccessor,
                   Local<Value>());

  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj"),
                  obj->NewInstance(env.local()).ToLocalChecked())
            .FromJust());

  // Use the throwing property setter/getter in a loop to force
  // the accessor ICs to be initialized.
  v8::Local<Value> result;
  result = Script::Compile(env.local(),
                           v8_str("var result = '';"
                                  "for (var i = 0; i < 5; i++) {"
                                  "  try { obj.x; } catch (e) { result += e; }"
                                  "}; result"))
               .ToLocalChecked()
               ->Run(env.local())
               .ToLocalChecked();
  CHECK(v8_str("ggggg")->Equals(env.local(), result).FromJust());

  result =
      Script::Compile(env.local(),
                      v8_str("var result = '';"
                             "for (var i = 0; i < 5; i++) {"
                             "  try { obj.x = i; } catch (e) { result += e; }"
                             "}; result"))
          .ToLocalChecked()
          ->Run(env.local())
          .ToLocalChecked();
  CHECK(v8_str("01234")->Equals(env.local(), result).FromJust());
}


static void AllocGetter(Local<String> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(v8::Array::New(info.GetIsolate(), 1000));
}


THREADED_TEST(Gc) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("xxx"), AllocGetter);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj"),
                  obj->NewInstance(env.local()).ToLocalChecked())
            .FromJust());
  Script::Compile(env.local(), v8_str("var last = [];"
                                      "for (var i = 0; i < 2048; i++) {"
                                      "  var result = obj.xxx;"
                                      "  result[0] = last;"
                                      "  last = result;"
                                      "}"))
      .ToLocalChecked()
      ->Run(env.local())
      .ToLocalChecked();
}


static void StackCheck(Local<String> name,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::StackFrameIterator iter(reinterpret_cast<i::Isolate*>(info.GetIsolate()));
  for (int i = 0; !iter.done(); i++) {
    i::StackFrame* frame = iter.frame();
    CHECK(i != 0 || (frame->type() == i::StackFrame::EXIT));
    i::Code* code = frame->LookupCode();
    CHECK(code->IsCode());
    i::Address pc = frame->pc();
    CHECK(code->contains(pc));
    iter.Advance();
  }
}


THREADED_TEST(StackIteration) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  i::StringStream::ClearMentionedObjectCache(
      reinterpret_cast<i::Isolate*>(isolate));
  obj->SetAccessor(v8_str("xxx"), StackCheck);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj"),
                  obj->NewInstance(env.local()).ToLocalChecked())
            .FromJust());
  Script::Compile(env.local(), v8_str("function foo() {"
                                      "  return obj.xxx;"
                                      "}"
                                      "for (var i = 0; i < 100; i++) {"
                                      "  foo();"
                                      "}"))
      .ToLocalChecked()
      ->Run(env.local())
      .ToLocalChecked();
}


static void AllocateHandles(Local<String> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info) {
  for (int i = 0; i < i::kHandleBlockSize + 1; i++) {
    v8::Local<v8::Value>::New(info.GetIsolate(), name);
  }
  info.GetReturnValue().Set(v8::Integer::New(info.GetIsolate(), 100));
}


THREADED_TEST(HandleScopeSegment) {
  // Check that we can return values past popping of handle scope
  // segments.
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("xxx"), AllocateHandles);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj"),
                  obj->NewInstance(env.local()).ToLocalChecked())
            .FromJust());
  v8::Local<v8::Value> result =
      Script::Compile(env.local(), v8_str("var result;"
                                          "for (var i = 0; i < 4; i++)"
                                          "  result = obj.xxx;"
                                          "result;"))
          .ToLocalChecked()
          ->Run(env.local())
          .ToLocalChecked();
  CHECK_EQ(100, result->Int32Value(env.local()).FromJust());
}


void JSONStringifyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  v8::Local<v8::Array> array = v8::Array::New(info.GetIsolate(), 1);
  CHECK(array->Set(info.GetIsolate()->GetCurrentContext(), 0, v8_str("regress"))
            .FromJust());
  info.GetReturnValue().Set(array);
}


void JSONStringifyGetter(Local<Name> name,
                         const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(v8_str("crbug-161028"));
}


THREADED_TEST(JSONStringifyNamedInterceptorObject) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetHandler(v8::NamedPropertyHandlerConfiguration(
      JSONStringifyGetter, NULL, NULL, NULL, JSONStringifyEnumerator));
  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj"),
                  obj->NewInstance(env.local()).ToLocalChecked())
            .FromJust());
  v8::Local<v8::String> expected = v8_str("{\"regress\":\"crbug-161028\"}");
  CHECK(CompileRun("JSON.stringify(obj)")
            ->Equals(env.local(), expected)
            .FromJust());
}


static v8::Local<v8::Context> expected_current_context;


static void check_contexts(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  CHECK(expected_current_context == info.GetIsolate()->GetCurrentContext());
}


THREADED_TEST(AccessorPropertyCrossContext) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Function> fun =
      v8::Function::New(env.local(), check_contexts).ToLocalChecked();
  LocalContext switch_context;
  CHECK(switch_context->Global()
            ->Set(switch_context.local(), v8_str("fun"), fun)
            .FromJust());
  v8::TryCatch try_catch(isolate);
  expected_current_context = env.local();
  CompileRun(
      "var o = Object.create(null, { n: { get:fun } });"
      "for (var i = 0; i < 10; i++) o.n;");
  CHECK(!try_catch.HasCaught());
}


THREADED_TEST(GlobalObjectAccessor) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  CompileRun(
      "var set_value = 1;"
      "Object.defineProperty(this.__proto__, 'x', {"
      "    get : function() { return this; },"
      "    set : function() { set_value = this; }"
      "});"
      "function getter() { return x; }"
      "function setter() { x = 1; }"
      "for (var i = 0; i < 4; i++) { getter(); setter(); }");
  CHECK(v8::Utils::OpenHandle(*CompileRun("getter()"))->IsJSGlobalProxy());
  CHECK(v8::Utils::OpenHandle(*CompileRun("set_value"))->IsJSGlobalProxy());
}


static void EmptyGetter(Local<Name> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
}


static void OneProperty(Local<String> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  info.GetReturnValue().Set(v8_num(1));
}


THREADED_TEST(Regress433458) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetHandler(v8::NamedPropertyHandlerConfiguration(EmptyGetter));
  obj->SetNativeDataProperty(v8_str("prop"), OneProperty);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj"),
                  obj->NewInstance(env.local()).ToLocalChecked())
            .FromJust());
  CompileRun(
      "Object.defineProperty(obj, 'prop', { writable: false });"
      "Object.defineProperty(obj, 'prop', { writable: true });");
}


static bool security_check_value = false;

static bool SecurityTestCallback(Local<v8::Context> accessing_context,
                                 Local<v8::Object> accessed_object,
                                 Local<v8::Value> data) {
  return security_check_value;
}


TEST(PrototypeGetterAccessCheck) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  auto fun_templ = v8::FunctionTemplate::New(isolate);
  auto getter_templ = v8::FunctionTemplate::New(isolate, handle_property);
  getter_templ->SetAcceptAnyReceiver(false);
  fun_templ->InstanceTemplate()->SetAccessorProperty(v8_str("foo"),
                                                     getter_templ);
  auto obj_templ = v8::ObjectTemplate::New(isolate);
  obj_templ->SetAccessCheckCallback(SecurityTestCallback);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("Fun"),
                  fun_templ->GetFunction(env.local()).ToLocalChecked())
            .FromJust());
  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj"),
                  obj_templ->NewInstance(env.local()).ToLocalChecked())
            .FromJust());
  CHECK(env->Global()
            ->Set(env.local(), v8_str("obj2"),
                  obj_templ->NewInstance(env.local()).ToLocalChecked())
            .FromJust());

  security_check_value = true;
  CompileRun("var proto = new Fun();");
  CompileRun("obj.__proto__ = proto;");
  ExpectInt32("proto.foo", 907);

  // Test direct.
  security_check_value = true;
  ExpectInt32("obj.foo", 907);
  security_check_value = false;
  {
    v8::TryCatch try_catch(isolate);
    CompileRun("obj.foo");
    CHECK(try_catch.HasCaught());
  }

  // Test through call.
  security_check_value = true;
  ExpectInt32("proto.__lookupGetter__('foo').call(obj)", 907);
  security_check_value = false;
  {
    v8::TryCatch try_catch(isolate);
    CompileRun("proto.__lookupGetter__('foo').call(obj)");
    CHECK(try_catch.HasCaught());
  }

  // Test ics.
  CompileRun(
      "function f() {"
      "   var x;"
      "  for (var i = 0; i < 4; i++) {"
      "    x = obj.foo;"
      "  }"
      "  return x;"
      "}");

  security_check_value = true;
  ExpectInt32("f()", 907);
  security_check_value = false;
  {
    v8::TryCatch try_catch(isolate);
    CompileRun("f();");
    CHECK(try_catch.HasCaught());
  }

  // Test crankshaft.
  CompileRun("%OptimizeFunctionOnNextCall(f);");

  security_check_value = true;
  ExpectInt32("f()", 907);
  security_check_value = false;
  {
    v8::TryCatch try_catch(isolate);
    CompileRun("f();");
    CHECK(try_catch.HasCaught());
  }
}

static void CheckReceiver(Local<String> name,
                          const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK(info.This()->IsObject());
}

TEST(Regress609134) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  auto fun_templ = v8::FunctionTemplate::New(isolate);
  fun_templ->InstanceTemplate()->SetNativeDataProperty(v8_str("foo"),
                                                       CheckReceiver);

  CHECK(env->Global()
            ->Set(env.local(), v8_str("Fun"),
                  fun_templ->GetFunction(env.local()).ToLocalChecked())
            .FromJust());

  CompileRun(
      "var f = new Fun();"
      "Number.prototype.__proto__ = f;"
      "var a = 42;"
      "for (var i = 0; i<3; i++) { a.foo; }");
}
