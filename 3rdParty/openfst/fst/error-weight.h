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
#ifndef FST_ERROR_WEIGHT_H_
#define FST_ERROR_WEIGHT_H_

#include <cstdint>
#include <ostream>
#include <string>

#include <fst/util.h>

namespace fst {

// A Weight that can never be instantiated. This is not a semi-ring.
// It is used for the arc type of empty FAR files.
struct ErrorWeight {
  using ReverseWeight = ErrorWeight;

  ErrorWeight() { FSTERROR() << "ErrorWeight::ErrorWeight called"; }

  uint64_t Hash() const { return 0; }
  bool Member() const { return false; }
  ErrorWeight Quantize(float = 0.0) const { return ErrorWeight(); }
  ReverseWeight Reverse() const { return ErrorWeight(); }
  void Write(std::ostream &) const { }

  static constexpr uint64_t Properties() { return 0; }
  static ErrorWeight Zero() { return ErrorWeight(); }
  static ErrorWeight One() { return ErrorWeight(); }
  static ErrorWeight NoWeight() { return ErrorWeight(); }

  static const std::string &Type() {
    static const auto *const type = new std::string("error");
    return *type;
  }
};

inline bool operator==(const ErrorWeight &, const ErrorWeight &) {
  return false;
}
inline bool operator!=(const ErrorWeight &, const ErrorWeight &) {
  return false;
}

inline bool ApproxEqual(const ErrorWeight &, const ErrorWeight &, float) {
  return false;
}
inline ErrorWeight Plus(const ErrorWeight &, const ErrorWeight &) {
  return ErrorWeight();
}
inline ErrorWeight Times(const ErrorWeight &, const ErrorWeight &) {
  return ErrorWeight();
}
inline ErrorWeight Divide(const ErrorWeight &, const ErrorWeight &) {
  return ErrorWeight();
}

inline std::ostream &operator<<(std::ostream &strm, const ErrorWeight &) {
  return strm;
}

}  // namespace fst

#endif  // FST_ERROR_WEIGHT_H_
