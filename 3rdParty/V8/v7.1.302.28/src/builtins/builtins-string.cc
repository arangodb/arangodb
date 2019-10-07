// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/conversions.h"
#include "src/counters.h"
#include "src/objects-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#endif
#include "src/regexp/regexp-utils.h"
#include "src/string-builder-inl.h"
#include "src/string-case.h"
#include "src/unicode-inl.h"
#include "src/unicode.h"

namespace v8 {
namespace internal {

namespace {  // for String.fromCodePoint

bool IsValidCodePoint(Isolate* isolate, Handle<Object> value) {
  if (!value->IsNumber() &&
      !Object::ToNumber(isolate, value).ToHandle(&value)) {
    return false;
  }

  if (Object::ToInteger(isolate, value).ToHandleChecked()->Number() !=
      value->Number()) {
    return false;
  }

  if (value->Number() < 0 || value->Number() > 0x10FFFF) {
    return false;
  }

  return true;
}

uc32 NextCodePoint(Isolate* isolate, BuiltinArguments args, int index) {
  Handle<Object> value = args.at(1 + index);
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, value,
                                   Object::ToNumber(isolate, value), -1);
  if (!IsValidCodePoint(isolate, value)) {
    isolate->Throw(*isolate->factory()->NewRangeError(
        MessageTemplate::kInvalidCodePoint, value));
    return -1;
  }
  return DoubleToUint32(value->Number());
}

}  // namespace

// ES6 section 21.1.2.2 String.fromCodePoint ( ...codePoints )
BUILTIN(StringFromCodePoint) {
  HandleScope scope(isolate);
  int const length = args.length() - 1;
  if (length == 0) return ReadOnlyRoots(isolate).empty_string();
  DCHECK_LT(0, length);

  // Optimistically assume that the resulting String contains only one byte
  // characters.
  std::vector<uint8_t> one_byte_buffer;
  one_byte_buffer.reserve(length);
  uc32 code = 0;
  int index;
  for (index = 0; index < length; index++) {
    code = NextCodePoint(isolate, args, index);
    if (code < 0) {
      return ReadOnlyRoots(isolate).exception();
    }
    if (code > String::kMaxOneByteCharCode) {
      break;
    }
    one_byte_buffer.push_back(code);
  }

  if (index == length) {
    RETURN_RESULT_OR_FAILURE(
        isolate, isolate->factory()->NewStringFromOneByte(Vector<uint8_t>(
                     one_byte_buffer.data(), one_byte_buffer.size())));
  }

  std::vector<uc16> two_byte_buffer;
  two_byte_buffer.reserve(length - index);

  while (true) {
    if (code <= static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
      two_byte_buffer.push_back(code);
    } else {
      two_byte_buffer.push_back(unibrow::Utf16::LeadSurrogate(code));
      two_byte_buffer.push_back(unibrow::Utf16::TrailSurrogate(code));
    }

    if (++index == length) {
      break;
    }
    code = NextCodePoint(isolate, args, index);
    if (code < 0) {
      return ReadOnlyRoots(isolate).exception();
    }
  }

  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->factory()->NewRawTwoByteString(
          static_cast<int>(one_byte_buffer.size() + two_byte_buffer.size())));

  CopyChars(result->GetChars(), one_byte_buffer.data(), one_byte_buffer.size());
  CopyChars(result->GetChars() + one_byte_buffer.size(), two_byte_buffer.data(),
            two_byte_buffer.size());

  return *result;
}

// ES6 section 21.1.3.6
// String.prototype.endsWith ( searchString [ , endPosition ] )
BUILTIN(StringPrototypeEndsWith) {
  HandleScope handle_scope(isolate);
  TO_THIS_STRING(str, "String.prototype.endsWith");

  // Check if the search string is a regExp and fail if it is.
  Handle<Object> search = args.atOrUndefined(isolate, 1);
  Maybe<bool> is_reg_exp = RegExpUtils::IsRegExp(isolate, search);
  if (is_reg_exp.IsNothing()) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  if (is_reg_exp.FromJust()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kFirstArgumentNotRegExp,
                              isolate->factory()->NewStringFromStaticChars(
                                  "String.prototype.endsWith")));
  }
  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  Handle<Object> position = args.atOrUndefined(isolate, 2);
  int end;

  if (position->IsUndefined(isolate)) {
    end = str->length();
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                       Object::ToInteger(isolate, position));
    end = str->ToValidIndex(*position);
  }

  int start = end - search_string->length();
  if (start < 0) return ReadOnlyRoots(isolate).false_value();

  str = String::Flatten(isolate, str);
  search_string = String::Flatten(isolate, search_string);

  DisallowHeapAllocation no_gc;  // ensure vectors stay valid
  String::FlatContent str_content = str->GetFlatContent();
  String::FlatContent search_content = search_string->GetFlatContent();

  if (str_content.IsOneByte() && search_content.IsOneByte()) {
    Vector<const uint8_t> str_vector = str_content.ToOneByteVector();
    Vector<const uint8_t> search_vector = search_content.ToOneByteVector();

    return isolate->heap()->ToBoolean(memcmp(str_vector.start() + start,
                                             search_vector.start(),
                                             search_string->length()) == 0);
  }

  FlatStringReader str_reader(isolate, str);
  FlatStringReader search_reader(isolate, search_string);

  for (int i = 0; i < search_string->length(); i++) {
    if (str_reader.Get(start + i) != search_reader.Get(i)) {
      return ReadOnlyRoots(isolate).false_value();
    }
  }
  return ReadOnlyRoots(isolate).true_value();
}

// ES6 section 21.1.3.9
// String.prototype.lastIndexOf ( searchString [ , position ] )
BUILTIN(StringPrototypeLastIndexOf) {
  HandleScope handle_scope(isolate);
  return String::LastIndexOf(isolate, args.receiver(),
                             args.atOrUndefined(isolate, 1),
                             args.atOrUndefined(isolate, 2));
}

// ES6 section 21.1.3.10 String.prototype.localeCompare ( that )
//
// This function is implementation specific.  For now, we do not
// do anything locale specific.
BUILTIN(StringPrototypeLocaleCompare) {
  HandleScope handle_scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kStringLocaleCompare);

#ifdef V8_INTL_SUPPORT
  TO_THIS_STRING(str1, "String.prototype.localeCompare");
  Handle<String> str2;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, str2, Object::ToString(isolate, args.atOrUndefined(isolate, 1)));
  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::StringLocaleCompare(isolate, str1, str2,
                                         args.atOrUndefined(isolate, 2),
                                         args.atOrUndefined(isolate, 3)));
#else
  DCHECK_EQ(2, args.length());

  TO_THIS_STRING(str1, "String.prototype.localeCompare");
  Handle<String> str2;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, str2,
                                     Object::ToString(isolate, args.at(1)));

  if (str1.is_identical_to(str2)) return Smi::kZero;  // Equal.
  int str1_length = str1->length();
  int str2_length = str2->length();

  // Decide trivial cases without flattening.
  if (str1_length == 0) {
    if (str2_length == 0) return Smi::kZero;  // Equal.
    return Smi::FromInt(-str2_length);
  } else {
    if (str2_length == 0) return Smi::FromInt(str1_length);
  }

  int end = str1_length < str2_length ? str1_length : str2_length;

  // No need to flatten if we are going to find the answer on the first
  // character. At this point we know there is at least one character
  // in each string, due to the trivial case handling above.
  int d = str1->Get(0) - str2->Get(0);
  if (d != 0) return Smi::FromInt(d);

  str1 = String::Flatten(isolate, str1);
  str2 = String::Flatten(isolate, str2);

  DisallowHeapAllocation no_gc;
  String::FlatContent flat1 = str1->GetFlatContent();
  String::FlatContent flat2 = str2->GetFlatContent();

  for (int i = 0; i < end; i++) {
    if (flat1.Get(i) != flat2.Get(i)) {
      return Smi::FromInt(flat1.Get(i) - flat2.Get(i));
    }
  }

  return Smi::FromInt(str1_length - str2_length);
#endif  // !V8_INTL_SUPPORT
}

#ifndef V8_INTL_SUPPORT
// ES6 section 21.1.3.12 String.prototype.normalize ( [form] )
//
// Simply checks the argument is valid and returns the string itself.
// If internationalization is enabled, then intl.js will override this function
// and provide the proper functionality, so this is just a fallback.
BUILTIN(StringPrototypeNormalize) {
  HandleScope handle_scope(isolate);
  TO_THIS_STRING(string, "String.prototype.normalize");

  Handle<Object> form_input = args.atOrUndefined(isolate, 1);
  if (form_input->IsUndefined(isolate)) return *string;

  Handle<String> form;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, form,
                                     Object::ToString(isolate, form_input));

  if (!(String::Equals(isolate, form,
                       isolate->factory()->NewStringFromStaticChars("NFC")) ||
        String::Equals(isolate, form,
                       isolate->factory()->NewStringFromStaticChars("NFD")) ||
        String::Equals(isolate, form,
                       isolate->factory()->NewStringFromStaticChars("NFKC")) ||
        String::Equals(isolate, form,
                       isolate->factory()->NewStringFromStaticChars("NFKD")))) {
    Handle<String> valid_forms =
        isolate->factory()->NewStringFromStaticChars("NFC, NFD, NFKC, NFKD");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewRangeError(MessageTemplate::kNormalizationForm, valid_forms));
  }

  return *string;
}
#endif  // !V8_INTL_SUPPORT

BUILTIN(StringPrototypeStartsWith) {
  HandleScope handle_scope(isolate);
  TO_THIS_STRING(str, "String.prototype.startsWith");

  // Check if the search string is a regExp and fail if it is.
  Handle<Object> search = args.atOrUndefined(isolate, 1);
  Maybe<bool> is_reg_exp = RegExpUtils::IsRegExp(isolate, search);
  if (is_reg_exp.IsNothing()) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  if (is_reg_exp.FromJust()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kFirstArgumentNotRegExp,
                              isolate->factory()->NewStringFromStaticChars(
                                  "String.prototype.startsWith")));
  }
  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  Handle<Object> position = args.atOrUndefined(isolate, 2);
  int start;

  if (position->IsUndefined(isolate)) {
    start = 0;
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                       Object::ToInteger(isolate, position));
    start = str->ToValidIndex(*position);
  }

  if (start + search_string->length() > str->length()) {
    return ReadOnlyRoots(isolate).false_value();
  }

  FlatStringReader str_reader(isolate, String::Flatten(isolate, str));
  FlatStringReader search_reader(isolate,
                                 String::Flatten(isolate, search_string));

  for (int i = 0; i < search_string->length(); i++) {
    if (str_reader.Get(start + i) != search_reader.Get(i)) {
      return ReadOnlyRoots(isolate).false_value();
    }
  }
  return ReadOnlyRoots(isolate).true_value();
}

#ifndef V8_INTL_SUPPORT
namespace {

inline bool ToUpperOverflows(uc32 character) {
  // y with umlauts and the micro sign are the only characters that stop
  // fitting into one-byte when converting to uppercase.
  static const uc32 yuml_code = 0xFF;
  static const uc32 micro_code = 0xB5;
  return (character == yuml_code || character == micro_code);
}

template <class Converter>
V8_WARN_UNUSED_RESULT static Object* ConvertCaseHelper(
    Isolate* isolate, String* string, SeqString* result, int result_length,
    unibrow::Mapping<Converter, 128>* mapping) {
  DisallowHeapAllocation no_gc;
  // We try this twice, once with the assumption that the result is no longer
  // than the input and, if that assumption breaks, again with the exact
  // length.  This may not be pretty, but it is nicer than what was here before
  // and I hereby claim my vaffel-is.
  //
  // NOTE: This assumes that the upper/lower case of an ASCII
  // character is also ASCII.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  bool has_changed_character = false;

  // Convert all characters to upper case, assuming that they will fit
  // in the buffer
  StringCharacterStream stream(string);
  unibrow::uchar chars[Converter::kMaxWidth];
  // We can assume that the string is not empty
  uc32 current = stream.GetNext();
  bool ignore_overflow = Converter::kIsToLower || result->IsSeqTwoByteString();
  for (int i = 0; i < result_length;) {
    bool has_next = stream.HasMore();
    uc32 next = has_next ? stream.GetNext() : 0;
    int char_length = mapping->get(current, next, chars);
    if (char_length == 0) {
      // The case conversion of this character is the character itself.
      result->Set(i, current);
      i++;
    } else if (char_length == 1 &&
               (ignore_overflow || !ToUpperOverflows(current))) {
      // Common case: converting the letter resulted in one character.
      DCHECK(static_cast<uc32>(chars[0]) != current);
      result->Set(i, chars[0]);
      has_changed_character = true;
      i++;
    } else if (result_length == string->length()) {
      bool overflows = ToUpperOverflows(current);
      // We've assumed that the result would be as long as the
      // input but here is a character that converts to several
      // characters.  No matter, we calculate the exact length
      // of the result and try the whole thing again.
      //
      // Note that this leaves room for optimization.  We could just
      // memcpy what we already have to the result string.  Also,
      // the result string is the last object allocated we could
      // "realloc" it and probably, in the vast majority of cases,
      // extend the existing string to be able to hold the full
      // result.
      int next_length = 0;
      if (has_next) {
        next_length = mapping->get(next, 0, chars);
        if (next_length == 0) next_length = 1;
      }
      int current_length = i + char_length + next_length;
      while (stream.HasMore()) {
        current = stream.GetNext();
        overflows |= ToUpperOverflows(current);
        // NOTE: we use 0 as the next character here because, while
        // the next character may affect what a character converts to,
        // it does not in any case affect the length of what it convert
        // to.
        int char_length = mapping->get(current, 0, chars);
        if (char_length == 0) char_length = 1;
        current_length += char_length;
        if (current_length > String::kMaxLength) {
          AllowHeapAllocation allocate_error_and_return;
          THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                         NewInvalidStringLengthError());
        }
      }
      // Try again with the real length.  Return signed if we need
      // to allocate a two-byte string for to uppercase.
      return (overflows && !ignore_overflow) ? Smi::FromInt(-current_length)
                                             : Smi::FromInt(current_length);
    } else {
      for (int j = 0; j < char_length; j++) {
        result->Set(i, chars[j]);
        i++;
      }
      has_changed_character = true;
    }
    current = next;
  }
  if (has_changed_character) {
    return result;
  } else {
    // If we didn't actually change anything in doing the conversion
    // we simple return the result and let the converted string
    // become garbage; there is no reason to keep two identical strings
    // alive.
    return string;
  }
}

template <class Converter>
V8_WARN_UNUSED_RESULT static Object* ConvertCase(
    Handle<String> s, Isolate* isolate,
    unibrow::Mapping<Converter, 128>* mapping) {
  s = String::Flatten(isolate, s);
  int length = s->length();
  // Assume that the string is not empty; we need this assumption later
  if (length == 0) return *s;

  // Simpler handling of ASCII strings.
  //
  // NOTE: This assumes that the upper/lower case of an ASCII
  // character is also ASCII.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  if (s->IsOneByteRepresentationUnderneath()) {
    // Same length as input.
    Handle<SeqOneByteString> result =
        isolate->factory()->NewRawOneByteString(length).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    String::FlatContent flat_content = s->GetFlatContent();
    DCHECK(flat_content.IsFlat());
    bool has_changed_character = false;
    int index_to_first_unprocessed = FastAsciiConvert<Converter::kIsToLower>(
        reinterpret_cast<char*>(result->GetChars()),
        reinterpret_cast<const char*>(flat_content.ToOneByteVector().start()),
        length, &has_changed_character);
    // If not ASCII, we discard the result and take the 2 byte path.
    if (index_to_first_unprocessed == length)
      return has_changed_character ? *result : *s;
  }

  Handle<SeqString> result;  // Same length as input.
  if (s->IsOneByteRepresentation()) {
    result = isolate->factory()->NewRawOneByteString(length).ToHandleChecked();
  } else {
    result = isolate->factory()->NewRawTwoByteString(length).ToHandleChecked();
  }

  Object* answer = ConvertCaseHelper(isolate, *s, *result, length, mapping);
  if (answer->IsException(isolate) || answer->IsString()) return answer;

  DCHECK(answer->IsSmi());
  length = Smi::ToInt(answer);
  if (s->IsOneByteRepresentation() && length > 0) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawOneByteString(length));
  } else {
    if (length < 0) length = -length;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawTwoByteString(length));
  }
  return ConvertCaseHelper(isolate, *s, *result, length, mapping);
}

}  // namespace

BUILTIN(StringPrototypeToLocaleLowerCase) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.toLocaleLowerCase");
  return ConvertCase(string, isolate,
                     isolate->runtime_state()->to_lower_mapping());
}

BUILTIN(StringPrototypeToLocaleUpperCase) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.toLocaleUpperCase");
  return ConvertCase(string, isolate,
                     isolate->runtime_state()->to_upper_mapping());
}

BUILTIN(StringPrototypeToLowerCase) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.toLowerCase");
  return ConvertCase(string, isolate,
                     isolate->runtime_state()->to_lower_mapping());
}

BUILTIN(StringPrototypeToUpperCase) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.toUpperCase");
  return ConvertCase(string, isolate,
                     isolate->runtime_state()->to_upper_mapping());
}
#endif  // !V8_INTL_SUPPORT

// ES6 #sec-string.prototype.raw
BUILTIN(StringRaw) {
  HandleScope scope(isolate);
  Handle<Object> templ = args.atOrUndefined(isolate, 1);
  const uint32_t argc = args.length();
  Handle<String> raw_string =
      isolate->factory()->NewStringFromAsciiChecked("raw");

  Handle<Object> cooked;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, cooked,
                                     Object::ToObject(isolate, templ));

  Handle<Object> raw;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, raw, Object::GetProperty(isolate, cooked, raw_string));
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, raw,
                                     Object::ToObject(isolate, raw));
  Handle<Object> raw_len;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, raw_len,
      Object::GetProperty(isolate, raw, isolate->factory()->length_string()));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, raw_len,
                                     Object::ToLength(isolate, raw_len));

  IncrementalStringBuilder result_builder(isolate);
  const uint32_t length = static_cast<uint32_t>(raw_len->Number());
  if (length > 0) {
    Handle<Object> first_element;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, first_element,
                                       Object::GetElement(isolate, raw, 0));

    Handle<String> first_string;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, first_string, Object::ToString(isolate, first_element));
    result_builder.AppendString(first_string);

    for (uint32_t i = 1, arg_i = 2; i < length; i++, arg_i++) {
      if (arg_i < argc) {
        Handle<String> argument_string;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, argument_string,
            Object::ToString(isolate, args.at(arg_i)));
        result_builder.AppendString(argument_string);
      }

      Handle<Object> element;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element,
                                         Object::GetElement(isolate, raw, i));

      Handle<String> element_string;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element_string,
                                         Object::ToString(isolate, element));
      result_builder.AppendString(element_string);
    }
  }

  RETURN_RESULT_OR_FAILURE(isolate, result_builder.Finish());
}

}  // namespace internal
}  // namespace v8
