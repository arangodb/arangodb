// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/encoded_string_vector.h"

using absl::MakeSpan;
using absl::Span;
using absl::string_view;
using std::vector;

namespace s2coding {

StringVectorEncoder::StringVectorEncoder() {
}

void StringVectorEncoder::Encode(Encoder* encoder) {
  offsets_.push_back(data_.length());
  // We don't encode the first element of "offsets_", which is always zero.
  EncodeUintVector<uint64>(MakeSpan(offsets_.data() + 1, &*offsets_.end()),
                                    encoder);
  encoder->Ensure(data_.length());
  encoder->putn(data_.base(), data_.length());
}

void StringVectorEncoder::Encode(Span<const string> v, Encoder* encoder) {
  StringVectorEncoder string_vector;
  for (const auto& str : v) string_vector.Add(str);
  string_vector.Encode(encoder);
}

bool EncodedStringVector::Init(Decoder* decoder) {
  if (!offsets_.Init(decoder)) return false;
  data_ = reinterpret_cast<const char*>(decoder->ptr());
  if (offsets_.size() > 0) {
    uint64 length = offsets_[offsets_.size() - 1];
    if (decoder->avail() < length) return false;
    decoder->skip(length);
  }
  return true;
}

vector<string_view> EncodedStringVector::Decode() const {
  size_t n = size();
  vector<string_view> result(n);
  for (int i = 0; i < n; ++i) {
    result[i] = (*this)[i];
  }
  return result;
}

}  // namespace s2coding
