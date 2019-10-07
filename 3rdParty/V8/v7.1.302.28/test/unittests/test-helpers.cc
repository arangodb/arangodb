// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-helpers.h"

#include "include/v8.h"
#include "src/api.h"
#include "src/base/template-utils.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/parsing/scanner.h"

namespace v8 {
namespace internal {
namespace test {

Handle<String> CreateSource(Isolate* isolate,
                            ExternalOneByteString::Resource* maybe_resource) {
  if (!maybe_resource) {
    static const char test_script[] = "(x) { x*x; }";
    maybe_resource = new test::ScriptResource(test_script, strlen(test_script));
  }
  return isolate->factory()
      ->NewExternalStringFromOneByte(maybe_resource)
      .ToHandleChecked();
}

Handle<SharedFunctionInfo> CreateSharedFunctionInfo(
    Isolate* isolate,
    v8::String::ExternalOneByteStringResource* maybe_resource) {
  HandleScope scope(isolate);
  Handle<String> source = CreateSource(isolate, maybe_resource);
  Handle<Script> script = isolate->factory()->NewScript(source);
  Handle<WeakFixedArray> infos = isolate->factory()->NewWeakFixedArray(3);
  script->set_shared_function_infos(*infos);
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->NewStringFromAsciiChecked("f"),
          Builtins::kCompileLazy);
  int function_literal_id = 1;
  // Ensure that the function can be compiled lazily.
  shared->set_uncompiled_data(
      *isolate->factory()->NewUncompiledDataWithoutPreParsedScope(
          ReadOnlyRoots(isolate).empty_string_handle(), 0, source->length(),
          function_literal_id));
  // Make sure we have an outer scope info, even though it's empty
  shared->set_raw_outer_scope_info_or_feedback_metadata(
      ScopeInfo::Empty(isolate));
  SharedFunctionInfo::SetScript(shared, script, function_literal_id);
  return scope.CloseAndEscape(shared);
}

std::unique_ptr<ParseInfo> OuterParseInfoForShared(
    Isolate* isolate, Handle<SharedFunctionInfo> shared) {
  Handle<Script> script =
      Handle<Script>::cast(handle(shared->script(), isolate));
  std::unique_ptr<ParseInfo> result =
      base::make_unique<ParseInfo>(isolate, script);

  // Create a character stream to simulate the parser having done so for the
  // to-level ParseProgram.
  Handle<String> source(String::cast(script->source()), isolate);
  std::unique_ptr<Utf16CharacterStream> stream(
      ScannerStream::For(isolate, source));
  result->set_character_stream(std::move(stream));

  return result;
}

}  // namespace test
}  // namespace internal
}  // namespace v8
