// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arguments-inl.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/regexp/regexp-utils.h"
#include "src/runtime/runtime-utils.h"
#include "src/strings/string-builder-inl.h"
#include "src/strings/string-search.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_GetSubstitution) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, matched, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
  CONVERT_SMI_ARG_CHECKED(position, 2);
  CONVERT_ARG_HANDLE_CHECKED(String, replacement, 3);
  CONVERT_SMI_ARG_CHECKED(start_index, 4);

  // A simple match without captures.
  class SimpleMatch : public String::Match {
   public:
    SimpleMatch(Handle<String> match, Handle<String> prefix,
                Handle<String> suffix)
        : match_(match), prefix_(prefix), suffix_(suffix) {}

    Handle<String> GetMatch() override { return match_; }
    Handle<String> GetPrefix() override { return prefix_; }
    Handle<String> GetSuffix() override { return suffix_; }

    int CaptureCount() override { return 0; }
    bool HasNamedCaptures() override { return false; }
    MaybeHandle<String> GetCapture(int i, bool* capture_exists) override {
      *capture_exists = false;
      return match_;  // Return arbitrary string handle.
    }
    MaybeHandle<String> GetNamedCapture(Handle<String> name,
                                        CaptureState* state) override {
      UNREACHABLE();
    }

   private:
    Handle<String> match_, prefix_, suffix_;
  };

  Handle<String> prefix =
      isolate->factory()->NewSubString(subject, 0, position);
  Handle<String> suffix = isolate->factory()->NewSubString(
      subject, position + matched->length(), subject->length());
  SimpleMatch match(matched, prefix, suffix);

  RETURN_RESULT_OR_FAILURE(
      isolate,
      String::GetSubstitution(isolate, &match, replacement, start_index));
}

// This may return an empty MaybeHandle if an exception is thrown or
// we abort due to reaching the recursion limit.
MaybeHandle<String> StringReplaceOneCharWithString(
    Isolate* isolate, Handle<String> subject, Handle<String> search,
    Handle<String> replace, bool* found, int recursion_limit) {
  StackLimitCheck stackLimitCheck(isolate);
  if (stackLimitCheck.HasOverflowed() || (recursion_limit == 0)) {
    return MaybeHandle<String>();
  }
  recursion_limit--;
  if (subject->IsConsString()) {
    ConsString cons = ConsString::cast(*subject);
    Handle<String> first = handle(cons.first(), isolate);
    Handle<String> second = handle(cons.second(), isolate);
    Handle<String> new_first;
    if (!StringReplaceOneCharWithString(isolate, first, search, replace, found,
                                        recursion_limit).ToHandle(&new_first)) {
      return MaybeHandle<String>();
    }
    if (*found) return isolate->factory()->NewConsString(new_first, second);

    Handle<String> new_second;
    if (!StringReplaceOneCharWithString(isolate, second, search, replace, found,
                                        recursion_limit)
             .ToHandle(&new_second)) {
      return MaybeHandle<String>();
    }
    if (*found) return isolate->factory()->NewConsString(first, new_second);

    return subject;
  } else {
    int index = String::IndexOf(isolate, subject, search, 0);
    if (index == -1) return subject;
    *found = true;
    Handle<String> first = isolate->factory()->NewSubString(subject, 0, index);
    Handle<String> cons1;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, cons1, isolate->factory()->NewConsString(first, replace),
        String);
    Handle<String> second =
        isolate->factory()->NewSubString(subject, index + 1, subject->length());
    return isolate->factory()->NewConsString(cons1, second);
  }
}

RUNTIME_FUNCTION(Runtime_StringReplaceOneCharWithString) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, search, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, replace, 2);

  // If the cons string tree is too deep, we simply abort the recursion and
  // retry with a flattened subject string.
  const int kRecursionLimit = 0x1000;
  bool found = false;
  Handle<String> result;
  if (StringReplaceOneCharWithString(isolate, subject, search, replace, &found,
                                     kRecursionLimit).ToHandle(&result)) {
    return *result;
  }
  if (isolate->has_pending_exception())
    return ReadOnlyRoots(isolate).exception();

  subject = String::Flatten(isolate, subject);
  if (StringReplaceOneCharWithString(isolate, subject, search, replace, &found,
                                     kRecursionLimit).ToHandle(&result)) {
    return *result;
  }
  if (isolate->has_pending_exception())
    return ReadOnlyRoots(isolate).exception();
  // In case of empty handle and no pending exception we have stack overflow.
  return isolate->StackOverflow();
}

RUNTIME_FUNCTION(Runtime_StringTrim) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<String> string = args.at<String>(0);
  CONVERT_SMI_ARG_CHECKED(mode, 1);
  String::TrimMode trim_mode = static_cast<String::TrimMode>(mode);
  return *String::Trim(isolate, string, trim_mode);
}

// ES6 #sec-string.prototype.includes
// String.prototype.includes(searchString [, position])
RUNTIME_FUNCTION(Runtime_StringIncludes) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  Handle<Object> receiver = args.at(0);
  if (receiver->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "String.prototype.includes")));
  }
  Handle<String> receiver_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_string,
                                     Object::ToString(isolate, receiver));

  // Check if the search string is a regExp and fail if it is.
  Handle<Object> search = args.at(1);
  Maybe<bool> is_reg_exp = RegExpUtils::IsRegExp(isolate, search);
  if (is_reg_exp.IsNothing()) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  if (is_reg_exp.FromJust()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kFirstArgumentNotRegExp,
                              isolate->factory()->NewStringFromStaticChars(
                                  "String.prototype.includes")));
  }
  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, args.at(1)));
  Handle<Object> position;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                     Object::ToInteger(isolate, args.at(2)));

  uint32_t index = receiver_string->ToValidIndex(*position);
  int index_in_str =
      String::IndexOf(isolate, receiver_string, search_string, index);
  return *isolate->factory()->ToBoolean(index_in_str != -1);
}

// ES6 #sec-string.prototype.indexof
// String.prototype.indexOf(searchString [, position])
RUNTIME_FUNCTION(Runtime_StringIndexOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  return String::IndexOf(isolate, args.at(0), args.at(1), args.at(2));
}

// ES6 #sec-string.prototype.indexof
// String.prototype.indexOf(searchString, position)
// Fast version that assumes that does not perform conversions of the incoming
// arguments.
RUNTIME_FUNCTION(Runtime_StringIndexOfUnchecked) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<String> receiver_string = args.at<String>(0);
  Handle<String> search_string = args.at<String>(1);
  int index = std::min(std::max(args.smi_at(2), 0), receiver_string->length());

  return Smi::FromInt(String::IndexOf(isolate, receiver_string, search_string,
                                      static_cast<uint32_t>(index)));
}

RUNTIME_FUNCTION(Runtime_StringLastIndexOf) {
  HandleScope handle_scope(isolate);
  return String::LastIndexOf(isolate, args.at(0), args.at(1),
                             isolate->factory()->undefined_value());
}

RUNTIME_FUNCTION(Runtime_StringSubstring) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  CONVERT_INT32_ARG_CHECKED(start, 1);
  CONVERT_INT32_ARG_CHECKED(end, 2);
  DCHECK_LE(0, start);
  DCHECK_LE(start, end);
  DCHECK_LE(end, string->length());
  isolate->counters()->sub_string_runtime()->Increment();
  return *isolate->factory()->NewSubString(string, start, end);
}

RUNTIME_FUNCTION(Runtime_StringAdd) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, str1, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, str2, 1);
  isolate->counters()->string_add_runtime()->Increment();
  RETURN_RESULT_OR_FAILURE(isolate,
                           isolate->factory()->NewConsString(str1, str2));
}


RUNTIME_FUNCTION(Runtime_InternalizeString) {
  HandleScope handles(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  return *isolate->factory()->InternalizeString(string);
}

RUNTIME_FUNCTION(Runtime_StringCharCodeAt) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, i, Uint32, args[1]);

  // Flatten the string.  If someone wants to get a char at an index
  // in a cons string, it is likely that more indices will be
  // accessed.
  subject = String::Flatten(isolate, subject);

  if (i >= static_cast<uint32_t>(subject->length())) {
    return ReadOnlyRoots(isolate).nan_value();
  }

  return Smi::FromInt(subject->Get(i));
}

RUNTIME_FUNCTION(Runtime_StringBuilderConcat) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  int32_t array_length;
  if (!args[1].ToInt32(&array_length)) {
    THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewInvalidStringLengthError());
  }
  CONVERT_ARG_HANDLE_CHECKED(String, special, 2);

  size_t actual_array_length = 0;
  CHECK(TryNumberToSize(array->length(), &actual_array_length));
  CHECK_GE(array_length, 0);
  CHECK(static_cast<size_t>(array_length) <= actual_array_length);

  // This assumption is used by the slice encoding in one or two smis.
  DCHECK_GE(Smi::kMaxValue, String::kMaxLength);

  CHECK(array->HasFastElements());
  JSObject::EnsureCanContainHeapObjectElements(array);

  int special_length = special->length();
  if (!array->HasObjectElements()) {
    return isolate->Throw(ReadOnlyRoots(isolate).illegal_argument_string());
  }

  int length;
  bool one_byte = special->IsOneByteRepresentation();

  {
    DisallowHeapAllocation no_gc;
    FixedArray fixed_array = FixedArray::cast(array->elements());
    if (fixed_array.length() < array_length) {
      array_length = fixed_array.length();
    }

    if (array_length == 0) {
      return ReadOnlyRoots(isolate).empty_string();
    } else if (array_length == 1) {
      Object first = fixed_array.get(0);
      if (first.IsString()) return first;
    }
    length = StringBuilderConcatLength(special_length, fixed_array,
                                       array_length, &one_byte);
  }

  if (length == -1) {
    return isolate->Throw(ReadOnlyRoots(isolate).illegal_argument_string());
  }
  if (length == 0) {
    return ReadOnlyRoots(isolate).empty_string();
  }

  if (one_byte) {
    Handle<SeqOneByteString> answer;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, answer, isolate->factory()->NewRawOneByteString(length));
    DisallowHeapAllocation no_gc;
    StringBuilderConcatHelper(*special, answer->GetChars(no_gc),
                              FixedArray::cast(array->elements()),
                              array_length);
    return *answer;
  } else {
    Handle<SeqTwoByteString> answer;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, answer, isolate->factory()->NewRawTwoByteString(length));
    DisallowHeapAllocation no_gc;
    StringBuilderConcatHelper(*special, answer->GetChars(no_gc),
                              FixedArray::cast(array->elements()),
                              array_length);
    return *answer;
  }
}


// Copies Latin1 characters to the given fixed array looking up
// one-char strings in the cache. Gives up on the first char that is
// not in the cache and fills the remainder with smi zeros. Returns
// the length of the successfully copied prefix.
static int CopyCachedOneByteCharsToArray(Heap* heap, const uint8_t* chars,
                                         FixedArray elements, int length) {
  DisallowHeapAllocation no_gc;
  FixedArray one_byte_cache = heap->single_character_string_cache();
  Object undefined = ReadOnlyRoots(heap).undefined_value();
  int i;
  WriteBarrierMode mode = elements.GetWriteBarrierMode(no_gc);
  for (i = 0; i < length; ++i) {
    Object value = one_byte_cache.get(chars[i]);
    if (value == undefined) break;
    elements.set(i, value, mode);
  }
  if (i < length) {
    MemsetTagged(elements.RawFieldOfElementAt(i), Smi::kZero, length - i);
  }
#ifdef DEBUG
  for (int j = 0; j < length; ++j) {
    Object element = elements.get(j);
    DCHECK(element == Smi::kZero ||
           (element.IsString() && String::cast(element).LooksValid()));
  }
#endif
  return i;
}

// Converts a String to JSArray.
// For example, "foo" => ["f", "o", "o"].
RUNTIME_FUNCTION(Runtime_StringToArray) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);

  s = String::Flatten(isolate, s);
  const int length = static_cast<int>(Min<uint32_t>(s->length(), limit));

  Handle<FixedArray> elements;
  int position = 0;
  if (s->IsFlat() && s->IsOneByteRepresentation()) {
    // Try using cached chars where possible.
    elements = isolate->factory()->NewUninitializedFixedArray(length);

    DisallowHeapAllocation no_gc;
    String::FlatContent content = s->GetFlatContent(no_gc);
    if (content.IsOneByte()) {
      Vector<const uint8_t> chars = content.ToOneByteVector();
      // Note, this will initialize all elements (not only the prefix)
      // to prevent GC from seeing partially initialized array.
      position = CopyCachedOneByteCharsToArray(isolate->heap(), chars.begin(),
                                               *elements, length);
    } else {
      MemsetTagged(elements->data_start(),
                   ReadOnlyRoots(isolate).undefined_value(), length);
    }
  } else {
    elements = isolate->factory()->NewFixedArray(length);
  }
  for (int i = position; i < length; ++i) {
    Handle<Object> str =
        isolate->factory()->LookupSingleCharacterStringFromCode(s->Get(i));
    elements->set(i, *str);
  }

#ifdef DEBUG
  for (int i = 0; i < length; ++i) {
    DCHECK_EQ(String::cast(elements->get(i)).length(), 1);
  }
#endif

  return *isolate->factory()->NewJSArrayWithElements(elements);
}

RUNTIME_FUNCTION(Runtime_StringLessThan) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
  ComparisonResult result = String::Compare(isolate, x, y);
  DCHECK_NE(result, ComparisonResult::kUndefined);
  return isolate->heap()->ToBoolean(
      ComparisonResultToBool(Operation::kLessThan, result));
}

RUNTIME_FUNCTION(Runtime_StringLessThanOrEqual) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
  ComparisonResult result = String::Compare(isolate, x, y);
  DCHECK_NE(result, ComparisonResult::kUndefined);
  return isolate->heap()->ToBoolean(
      ComparisonResultToBool(Operation::kLessThanOrEqual, result));
}

RUNTIME_FUNCTION(Runtime_StringGreaterThan) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
  ComparisonResult result = String::Compare(isolate, x, y);
  DCHECK_NE(result, ComparisonResult::kUndefined);
  return isolate->heap()->ToBoolean(
      ComparisonResultToBool(Operation::kGreaterThan, result));
}

RUNTIME_FUNCTION(Runtime_StringGreaterThanOrEqual) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
  ComparisonResult result = String::Compare(isolate, x, y);
  DCHECK_NE(result, ComparisonResult::kUndefined);
  return isolate->heap()->ToBoolean(
      ComparisonResultToBool(Operation::kGreaterThanOrEqual, result));
}

RUNTIME_FUNCTION(Runtime_StringEqual) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
  return isolate->heap()->ToBoolean(String::Equals(isolate, x, y));
}

RUNTIME_FUNCTION(Runtime_FlattenString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, str, 0);
  return *String::Flatten(isolate, str);
}

RUNTIME_FUNCTION(Runtime_StringMaxLength) {
  SealHandleScope shs(isolate);
  return Smi::FromInt(String::kMaxLength);
}

RUNTIME_FUNCTION(Runtime_StringCompareSequence) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, search_string, 1);
  CONVERT_NUMBER_CHECKED(int, start, Int32, args[2]);

  // Check if start + searchLength is in bounds.
  DCHECK_LE(start + search_string->length(), string->length());

  FlatStringReader string_reader(isolate, String::Flatten(isolate, string));
  FlatStringReader search_reader(isolate,
                                 String::Flatten(isolate, search_string));

  for (int i = 0; i < search_string->length(); i++) {
    if (string_reader.Get(start + i) != search_reader.Get(i)) {
      return ReadOnlyRoots(isolate).false_value();
    }
  }

  return ReadOnlyRoots(isolate).true_value();
}

RUNTIME_FUNCTION(Runtime_StringEscapeQuotes) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);

  // Equivalent to global replacement `string.replace(/"/g, "&quot")`, but this
  // does not modify any global state (e.g. the regexp match info).

  const int string_length = string->length();
  Handle<String> quotes =
      isolate->factory()->LookupSingleCharacterStringFromCode('"');

  int index = String::IndexOf(isolate, string, quotes, 0);

  // No quotes, nothing to do.
  if (index == -1) return *string;

  // Find all quotes.
  std::vector<int> indices = {index};
  while (index + 1 < string_length) {
    index = String::IndexOf(isolate, string, quotes, index + 1);
    if (index == -1) break;
    indices.emplace_back(index);
  }

  // Build the replacement string.
  Handle<String> replacement =
      isolate->factory()->NewStringFromAsciiChecked("&quot;");
  const int estimated_part_count = static_cast<int>(indices.size()) * 2 + 1;
  ReplacementStringBuilder builder(isolate->heap(), string,
                                   estimated_part_count);

  int prev_index = -1;  // Start at -1 to avoid special-casing the first match.
  for (int index : indices) {
    const int slice_start = prev_index + 1;
    const int slice_end = index;
    if (slice_end > slice_start) {
      builder.AddSubjectSlice(slice_start, slice_end);
    }
    builder.AddString(replacement);
    prev_index = index;
  }

  if (prev_index < string_length - 1) {
    builder.AddSubjectSlice(prev_index + 1, string_length);
  }

  return *builder.ToString().ToHandleChecked();
}

}  // namespace internal
}  // namespace v8
