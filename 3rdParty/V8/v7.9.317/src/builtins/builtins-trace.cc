// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/json/json-stringifier.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

using v8::tracing::TracedValue;

#define MAX_STACK_LENGTH 100

class MaybeUtf8 {
 public:
  explicit MaybeUtf8(Isolate* isolate, Handle<String> string) : buf_(data_) {
    string = String::Flatten(isolate, string);
    int len;
    if (string->IsOneByteRepresentation()) {
      // Technically this allows unescaped latin1 characters but the trace
      // events mechanism currently does the same and the current consuming
      // tools are tolerant of it. A more correct approach here would be to
      // escape non-ascii characters but this is easier and faster.
      len = string->length();
      AllocateSufficientSpace(len);
      if (len > 0) {
        // Why copy? Well, the trace event mechanism requires null-terminated
        // strings, the bytes we get from SeqOneByteString are not. buf_ is
        // guaranteed to be null terminated.
        DisallowHeapAllocation no_gc;
        memcpy(buf_, Handle<SeqOneByteString>::cast(string)->GetChars(no_gc),
               len);
      }
    } else {
      Local<v8::String> local = Utils::ToLocal(string);
      auto* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
      len = local->Utf8Length(v8_isolate);
      AllocateSufficientSpace(len);
      if (len > 0) {
        local->WriteUtf8(v8_isolate, reinterpret_cast<char*>(buf_));
      }
    }
    buf_[len] = 0;
  }
  const char* operator*() const { return reinterpret_cast<const char*>(buf_); }

 private:
  void AllocateSufficientSpace(int len) {
    if (len + 1 > MAX_STACK_LENGTH) {
      allocated_.reset(new uint8_t[len + 1]);
      buf_ = allocated_.get();
    }
  }

  // In the most common cases, the buffer here will be stack allocated.
  // A heap allocation will only occur if the data is more than MAX_STACK_LENGTH
  // Given that this is used primarily for trace event categories and names,
  // the MAX_STACK_LENGTH should be more than enough.
  uint8_t* buf_;
  uint8_t data_[MAX_STACK_LENGTH];
  std::unique_ptr<uint8_t> allocated_;
};

class JsonTraceValue : public ConvertableToTraceFormat {
 public:
  explicit JsonTraceValue(Isolate* isolate, Handle<String> object) {
    // object is a JSON string serialized using JSON.stringify() from within
    // the BUILTIN(Trace) method. This may (likely) contain UTF8 values so
    // to grab the appropriate buffer data we have to serialize it out. We
    // hold on to the bits until the AppendAsTraceFormat method is called.
    MaybeUtf8 data(isolate, object);
    data_ = *data;
  }

  void AppendAsTraceFormat(std::string* out) const override { *out += data_; }

 private:
  std::string data_;
};

const uint8_t* GetCategoryGroupEnabled(Isolate* isolate,
                                       Handle<String> string) {
  MaybeUtf8 category(isolate, string);
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(*category);
}

#undef MAX_STACK_LENGTH

}  // namespace

// Builins::kIsTraceCategoryEnabled(category) : bool
BUILTIN(IsTraceCategoryEnabled) {
  HandleScope scope(isolate);
  Handle<Object> category = args.atOrUndefined(isolate, 1);
  if (!category->IsString()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kTraceEventCategoryError));
  }
  return isolate->heap()->ToBoolean(
      *GetCategoryGroupEnabled(isolate, Handle<String>::cast(category)));
}

// Builtins::kTrace(phase, category, name, id, data) : bool
BUILTIN(Trace) {
  HandleScope handle_scope(isolate);

  Handle<Object> phase_arg = args.atOrUndefined(isolate, 1);
  Handle<Object> category = args.atOrUndefined(isolate, 2);
  Handle<Object> name_arg = args.atOrUndefined(isolate, 3);
  Handle<Object> id_arg = args.atOrUndefined(isolate, 4);
  Handle<Object> data_arg = args.atOrUndefined(isolate, 5);

  const uint8_t* category_group_enabled =
      GetCategoryGroupEnabled(isolate, Handle<String>::cast(category));

  // Exit early if the category group is not enabled.
  if (!*category_group_enabled) {
    return ReadOnlyRoots(isolate).false_value();
  }

  if (!phase_arg->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kTraceEventPhaseError));
  }
  if (!category->IsString()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kTraceEventCategoryError));
  }
  if (!name_arg->IsString()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kTraceEventNameError));
  }

  uint32_t flags = TRACE_EVENT_FLAG_COPY;
  int32_t id = 0;
  if (!id_arg->IsNullOrUndefined(isolate)) {
    if (!id_arg->IsNumber()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kTraceEventIDError));
    }
    flags |= TRACE_EVENT_FLAG_HAS_ID;
    id = DoubleToInt32(id_arg->Number());
  }

  Handle<String> name_str = Handle<String>::cast(name_arg);
  if (name_str->length() == 0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kTraceEventNameLengthError));
  }
  MaybeUtf8 name(isolate, name_str);

  // We support passing one additional trace event argument with the
  // name "data". Any JSON serializable value may be passed.
  static const char* arg_name = "data";
  int32_t num_args = 0;
  uint8_t arg_type;
  uint64_t arg_value;

  if (!data_arg->IsUndefined(isolate)) {
    // Serializes the data argument as a JSON string, which is then
    // copied into an object. This eliminates duplicated code but
    // could have perf costs. It is also subject to all the same
    // limitations as JSON.stringify() as it relates to circular
    // references and value limitations (e.g. BigInt is not supported).
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        JsonStringify(isolate, data_arg, isolate->factory()->undefined_value(),
                      isolate->factory()->undefined_value()));
    std::unique_ptr<JsonTraceValue> traced_value;
    traced_value.reset(
        new JsonTraceValue(isolate, Handle<String>::cast(result)));
    tracing::SetTraceValue(std::move(traced_value), &arg_type, &arg_value);
    num_args++;
  }

  TRACE_EVENT_API_ADD_TRACE_EVENT(
      static_cast<char>(DoubleToInt32(phase_arg->Number())),
      category_group_enabled, *name, tracing::kGlobalScope, id, tracing::kNoId,
      num_args, &arg_name, &arg_type, &arg_value, flags);

  return ReadOnlyRoots(isolate).true_value();
}

}  // namespace internal
}  // namespace v8
