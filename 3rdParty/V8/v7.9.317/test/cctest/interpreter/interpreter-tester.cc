// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/interpreter/interpreter-tester.h"

#include "src/api/api-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

MaybeHandle<Object> CallInterpreter(Isolate* isolate,
                                    Handle<JSFunction> function) {
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), 0, nullptr);
}

InterpreterTester::InterpreterTester(
    Isolate* isolate, const char* source, MaybeHandle<BytecodeArray> bytecode,
    MaybeHandle<FeedbackMetadata> feedback_metadata, const char* filter)
    : isolate_(isolate),
      source_(source),
      bytecode_(bytecode),
      feedback_metadata_(feedback_metadata) {
  i::FLAG_always_opt = false;
}

InterpreterTester::InterpreterTester(
    Isolate* isolate, Handle<BytecodeArray> bytecode,
    MaybeHandle<FeedbackMetadata> feedback_metadata, const char* filter)
    : InterpreterTester(isolate, nullptr, bytecode, feedback_metadata, filter) {
}

InterpreterTester::InterpreterTester(Isolate* isolate, const char* source,
                                     const char* filter)
    : InterpreterTester(isolate, source, MaybeHandle<BytecodeArray>(),
                        MaybeHandle<FeedbackMetadata>(), filter) {}

InterpreterTester::~InterpreterTester() = default;

Local<Message> InterpreterTester::CheckThrowsReturnMessage() {
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate_));
  auto callable = GetCallable<>();
  MaybeHandle<Object> no_result = callable();
  CHECK(isolate_->has_pending_exception());
  CHECK(try_catch.HasCaught());
  CHECK(no_result.is_null());
  isolate_->OptionalRescheduleException(true);
  CHECK(!try_catch.Message().IsEmpty());
  return try_catch.Message();
}

Handle<Object> InterpreterTester::NewObject(const char* script) {
  return v8::Utils::OpenHandle(*CompileRun(script));
}

Handle<String> InterpreterTester::GetName(Isolate* isolate, const char* name) {
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(name);
  return isolate->factory()->string_table()->LookupString(isolate, result);
}

std::string InterpreterTester::SourceForBody(const char* body) {
  return "function " + function_name() + "() {\n" + std::string(body) + "\n}";
}

std::string InterpreterTester::function_name() {
  return std::string(kFunctionName);
}

const char InterpreterTester::kFunctionName[] = "f";

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
