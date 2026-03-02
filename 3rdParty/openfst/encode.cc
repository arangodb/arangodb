// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Definitions for encode table header.

#include <fst/encode.h>

#include <cstdint>

namespace fst {

bool EncodeTableHeader::Read(std::istream &strm, const std::string &source) {
  int32_t magic_number;
  ReadType(strm, &magic_number);
  if (magic_number != internal::kEncodeMagicNumber) {
    LOG(ERROR) << "EncodeTableHeader::Read: Bad encode table header: " << source
               << ". Magic number not matched. Got: " << magic_number;
    return false;
  }
  ReadType(strm, &arctype_);
  ReadType(strm, &flags_);
  ReadType(strm, &size_);
  if (!strm) {
    LOG(ERROR) << "EncodeTableHeader::Read: Read failed: " << source;
    return false;
  }
  return true;
}

bool EncodeTableHeader::Write(std::ostream &strm,
                              const std::string &source) const {
  WriteType(strm, internal::kEncodeMagicNumber);
  WriteType(strm, arctype_);
  WriteType(strm, flags_);
  WriteType(strm, size_);
  strm.flush();
  if (!strm) {
    LOG(ERROR) << "EncodeTableHeader::Write: Write failed: " << source;
    return false;
  }
  return true;
}

}  // namespace fst
