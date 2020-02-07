// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/strings/string-builder-inl.h"

#include "src/execution/isolate-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/js-array-inl.h"

namespace v8 {
namespace internal {

template <typename sinkchar>
void StringBuilderConcatHelper(String special, sinkchar* sink,
                               FixedArray fixed_array, int array_length) {
  DisallowHeapAllocation no_gc;
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    Object element = fixed_array.get(i);
    if (element.IsSmi()) {
      // Smi encoding of position and length.
      int encoded_slice = Smi::ToInt(element);
      int pos;
      int len;
      if (encoded_slice > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(encoded_slice);
        len = StringBuilderSubstringLength::decode(encoded_slice);
      } else {
        // Position and length encoded in two smis.
        Object obj = fixed_array.get(++i);
        DCHECK(obj.IsSmi());
        pos = Smi::ToInt(obj);
        len = -encoded_slice;
      }
      String::WriteToFlat(special, sink + position, pos, pos + len);
      position += len;
    } else {
      String string = String::cast(element);
      int element_length = string.length();
      String::WriteToFlat(string, sink + position, 0, element_length);
      position += element_length;
    }
  }
}

template void StringBuilderConcatHelper<uint8_t>(String special, uint8_t* sink,
                                                 FixedArray fixed_array,
                                                 int array_length);

template void StringBuilderConcatHelper<uc16>(String special, uc16* sink,
                                              FixedArray fixed_array,
                                              int array_length);

int StringBuilderConcatLength(int special_length, FixedArray fixed_array,
                              int array_length, bool* one_byte) {
  DisallowHeapAllocation no_gc;
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    int increment = 0;
    Object elt = fixed_array.get(i);
    if (elt.IsSmi()) {
      // Smi encoding of position and length.
      int smi_value = Smi::ToInt(elt);
      int pos;
      int len;
      if (smi_value > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(smi_value);
        len = StringBuilderSubstringLength::decode(smi_value);
      } else {
        // Position and length encoded in two smis.
        len = -smi_value;
        // Get the position and check that it is a positive smi.
        i++;
        if (i >= array_length) return -1;
        Object next_smi = fixed_array.get(i);
        if (!next_smi.IsSmi()) return -1;
        pos = Smi::ToInt(next_smi);
        if (pos < 0) return -1;
      }
      DCHECK_GE(pos, 0);
      DCHECK_GE(len, 0);
      if (pos > special_length || len > special_length - pos) return -1;
      increment = len;
    } else if (elt.IsString()) {
      String element = String::cast(elt);
      int element_length = element.length();
      increment = element_length;
      if (*one_byte && !element.IsOneByteRepresentation()) {
        *one_byte = false;
      }
    } else {
      return -1;
    }
    if (increment > String::kMaxLength - position) {
      return kMaxInt;  // Provoke throw on allocation.
    }
    position += increment;
  }
  return position;
}

FixedArrayBuilder::FixedArrayBuilder(Isolate* isolate, int initial_capacity)
    : array_(isolate->factory()->NewFixedArrayWithHoles(initial_capacity)),
      length_(0),
      has_non_smi_elements_(false) {
  // Require a non-zero initial size. Ensures that doubling the size to
  // extend the array will work.
  DCHECK_GT(initial_capacity, 0);
}

FixedArrayBuilder::FixedArrayBuilder(Handle<FixedArray> backing_store)
    : array_(backing_store), length_(0), has_non_smi_elements_(false) {
  // Require a non-zero initial size. Ensures that doubling the size to
  // extend the array will work.
  DCHECK_GT(backing_store->length(), 0);
}

bool FixedArrayBuilder::HasCapacity(int elements) {
  int length = array_->length();
  int required_length = length_ + elements;
  return (length >= required_length);
}

void FixedArrayBuilder::EnsureCapacity(Isolate* isolate, int elements) {
  int length = array_->length();
  int required_length = length_ + elements;
  if (length < required_length) {
    int new_length = length;
    do {
      new_length *= 2;
    } while (new_length < required_length);
    Handle<FixedArray> extended_array =
        isolate->factory()->NewFixedArrayWithHoles(new_length);
    array_->CopyTo(0, *extended_array, 0, length_);
    array_ = extended_array;
  }
}

void FixedArrayBuilder::Add(Object value) {
  DCHECK(!value.IsSmi());
  array_->set(length_, value);
  length_++;
  has_non_smi_elements_ = true;
}

void FixedArrayBuilder::Add(Smi value) {
  DCHECK(value.IsSmi());
  array_->set(length_, value);
  length_++;
}

int FixedArrayBuilder::capacity() { return array_->length(); }

Handle<JSArray> FixedArrayBuilder::ToJSArray(Handle<JSArray> target_array) {
  JSArray::SetContent(target_array, array_);
  target_array->set_length(Smi::FromInt(length_));
  return target_array;
}

ReplacementStringBuilder::ReplacementStringBuilder(Heap* heap,
                                                   Handle<String> subject,
                                                   int estimated_part_count)
    : heap_(heap),
      array_builder_(Isolate::FromHeap(heap), estimated_part_count),
      subject_(subject),
      character_count_(0),
      is_one_byte_(subject->IsOneByteRepresentation()) {
  // Require a non-zero initial size. Ensures that doubling the size to
  // extend the array will work.
  DCHECK_GT(estimated_part_count, 0);
}

void ReplacementStringBuilder::EnsureCapacity(int elements) {
  array_builder_.EnsureCapacity(Isolate::FromHeap(heap_), elements);
}

void ReplacementStringBuilder::AddString(Handle<String> string) {
  int length = string->length();
  DCHECK_GT(length, 0);
  AddElement(string);
  if (!string->IsOneByteRepresentation()) {
    is_one_byte_ = false;
  }
  IncrementCharacterCount(length);
}

MaybeHandle<String> ReplacementStringBuilder::ToString() {
  Isolate* isolate = Isolate::FromHeap(heap_);
  if (array_builder_.length() == 0) {
    return isolate->factory()->empty_string();
  }

  Handle<String> joined_string;
  if (is_one_byte_) {
    Handle<SeqOneByteString> seq;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, seq, isolate->factory()->NewRawOneByteString(character_count_),
        String);

    DisallowHeapAllocation no_gc;
    uint8_t* char_buffer = seq->GetChars(no_gc);
    StringBuilderConcatHelper(*subject_, char_buffer, *array_builder_.array(),
                              array_builder_.length());
    joined_string = Handle<String>::cast(seq);
  } else {
    // Two-byte.
    Handle<SeqTwoByteString> seq;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, seq, isolate->factory()->NewRawTwoByteString(character_count_),
        String);

    DisallowHeapAllocation no_gc;
    uc16* char_buffer = seq->GetChars(no_gc);
    StringBuilderConcatHelper(*subject_, char_buffer, *array_builder_.array(),
                              array_builder_.length());
    joined_string = Handle<String>::cast(seq);
  }
  return joined_string;
}

void ReplacementStringBuilder::AddElement(Handle<Object> element) {
  DCHECK(element->IsSmi() || element->IsString());
  EnsureCapacity(1);
  DisallowHeapAllocation no_gc;
  array_builder_.Add(*element);
}

IncrementalStringBuilder::IncrementalStringBuilder(Isolate* isolate)
    : isolate_(isolate),
      encoding_(String::ONE_BYTE_ENCODING),
      overflowed_(false),
      part_length_(kInitialPartLength),
      current_index_(0) {
  // Create an accumulator handle starting with the empty string.
  accumulator_ =
      Handle<String>::New(ReadOnlyRoots(isolate).empty_string(), isolate);
  current_part_ =
      factory()->NewRawOneByteString(part_length_).ToHandleChecked();
}

int IncrementalStringBuilder::Length() const {
  return accumulator_->length() + current_index_;
}

void IncrementalStringBuilder::Accumulate(Handle<String> new_part) {
  Handle<String> new_accumulator;
  if (accumulator()->length() + new_part->length() > String::kMaxLength) {
    // Set the flag and carry on. Delay throwing the exception till the end.
    new_accumulator = factory()->empty_string();
    overflowed_ = true;
  } else {
    new_accumulator =
        factory()->NewConsString(accumulator(), new_part).ToHandleChecked();
  }
  set_accumulator(new_accumulator);
}

void IncrementalStringBuilder::Extend() {
  DCHECK_EQ(current_index_, current_part()->length());
  Accumulate(current_part());
  if (part_length_ <= kMaxPartLength / kPartLengthGrowthFactor) {
    part_length_ *= kPartLengthGrowthFactor;
  }
  Handle<String> new_part;
  if (encoding_ == String::ONE_BYTE_ENCODING) {
    new_part = factory()->NewRawOneByteString(part_length_).ToHandleChecked();
  } else {
    new_part = factory()->NewRawTwoByteString(part_length_).ToHandleChecked();
  }
  // Reuse the same handle to avoid being invalidated when exiting handle scope.
  set_current_part(new_part);
  current_index_ = 0;
}

MaybeHandle<String> IncrementalStringBuilder::Finish() {
  ShrinkCurrentPart();
  Accumulate(current_part());
  if (overflowed_) {
    THROW_NEW_ERROR(isolate_, NewInvalidStringLengthError(), String);
  }
  return accumulator();
}

// Short strings can be copied directly to {current_part_}.
// Requires the IncrementalStringBuilder to either have two byte encoding or
// the incoming string to have one byte representation "underneath" (The
// one byte check requires the string to be flat).
bool IncrementalStringBuilder::CanAppendByCopy(Handle<String> string) {
  constexpr int kMaxStringLengthForCopy = 16;
  const bool representation_ok =
      encoding_ == String::TWO_BYTE_ENCODING ||
      (string->IsFlat() && String::IsOneByteRepresentationUnderneath(*string));

  return representation_ok && string->length() <= kMaxStringLengthForCopy &&
         CurrentPartCanFit(string->length());
}

void IncrementalStringBuilder::AppendStringByCopy(Handle<String> string) {
  DCHECK(CanAppendByCopy(string));

  Handle<SeqOneByteString> part =
      Handle<SeqOneByteString>::cast(current_part());
  {
    DisallowHeapAllocation no_gc;
    String::WriteToFlat(*string, part->GetChars(no_gc) + current_index_, 0,
                        string->length());
  }
  current_index_ += string->length();
  DCHECK(current_index_ <= part_length_);
  if (current_index_ == part_length_) Extend();
}

void IncrementalStringBuilder::AppendString(Handle<String> string) {
  if (CanAppendByCopy(string)) {
    AppendStringByCopy(string);
    return;
  }

  ShrinkCurrentPart();
  part_length_ = kInitialPartLength;  // Allocate conservatively.
  Extend();  // Attach current part and allocate new part.
  Accumulate(string);
}
}  // namespace internal
}  // namespace v8
