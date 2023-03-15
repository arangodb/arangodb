// Copyright 2005 Google Inc. All Rights Reserved.
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

//
//   far-far-superior implementation courtesy of amc@google.com (Adam Costello)
//
// Nth Derivative Coding
//   (In signal processing disciplines, this is known as N-th Delta Coding.)
//
// Good for varint coding integer sequences with polynomial trends.
//
// Instead of coding a sequence of values directly, code its nth-order discrete
// derivative.  Overflow in integer addition and subtraction makes this a
// lossless transform.
//
//                                       constant     linear      quadratic
//                                        trend       trend         trend
//                                      /        \  /        \  /           \_
// input                               |0  0  0  0  1  2  3  4  9  16  25  36
// 0th derivative(identity)            |0  0  0  0  1  2  3  4  9  16  25  36
// 1st derivative(delta coding)        |   0  0  0  1  1  1  1  5   7   9  11
// 2nd derivative(linear prediction)   |      0  0  1  0  0  0  4   2   2   2
//                                      -------------------------------------
//                                      0  1  2  3  4  5  6  7  8   9  10  11
//                                                  n in sequence
//
// Higher-order codings can break even or be detrimental on other sequences.
//
//                                           random            oscillating
//                                      /               \  /                  \_
// input                               |5  9  6  1   8  8  2 -2   4  -4   6  -6
// 0th derivative(identity)            |5  9  6  1   8  8  2 -2   4  -4   6  -6
// 1st derivative(delta coding)        |   4 -3 -5   7  0 -6 -4   6  -8  10 -12
// 2nd derivative(linear prediction)   |     -7 -2  12 -7 -6  2  10 -14  18 -22
//                                      ---------------------------------------
//                                      0  1  2  3  4   5  6  7   8   9  10  11
//                                                  n in sequence
//
// Note that the nth derivative isn't available until sequence item n.  Earlier
// values are coded at lower order.  For the above table, read 5 4 -7 -2 12 ...
//
// A caveat on class usage.  Encode() and Decode() share state.  Using both
// without a Reset() in-between probably doesn't make sense.

#ifndef S2_UTIL_CODING_NTH_DERIVATIVE_H_
#define S2_UTIL_CODING_NTH_DERIVATIVE_H_

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"

class NthDerivativeCoder {
 public:
  // range of supported Ns: [ N_MIN, N_MAX ]
  enum {
    N_MIN = 0,
    N_MAX = 10,
  };

  // Initialize a new NthDerivativeCoder of the given N.
  explicit NthDerivativeCoder(int n);

  // Encode the next value in the sequence.  Don't mix with Decode() calls.
  int32 Encode(int32 k);

  // Decode the next value in the sequence.  Don't mix with Encode() calls.
  int32 Decode(int32 k);

  // Reset state.
  void Reset();

  // accessors
  int n() const { return n_; }

 private:
  int n_;  // derivative order of the coder (the N in NthDerivative)
  int m_;  // the derivative order in which to code the next value(ramps to n_)
  int32 memory_[N_MAX];  // value memory. [0] is oldest
};

// Implementation below.  Callers Ignore.
//
// Inlining the implementation roughly doubled the speed.  All other
// optimization tricks failed miserably.

#if ~0 != -1
#error Sorry, this code needs twos complement integers.
#endif

inline NthDerivativeCoder::NthDerivativeCoder(int n) : n_(n) {
  if (n < N_MIN || n > N_MAX) {
    S2_LOG(ERROR) << "Unsupported N: " << n << ".  Using 0 instead.";
    n_ = 0;
  }
  Reset();
}

inline int32 NthDerivativeCoder::Encode(int32 k) {
  for (int i = 0; i < m_; ++i) {
    uint32 delta = static_cast<uint32>(k) - memory_[i];
    memory_[i] = k;
    k = delta;
  }
  if (m_ < n_)
    memory_[m_++] = k;
  return k;
}

inline int32 NthDerivativeCoder::Decode(int32 k) {
  if (m_ < n_)
    m_++;
  for (int i = m_ - 1; i >= 0; --i)
    k = memory_[i] = memory_[i] + static_cast<uint32>(k);
  return k;
}

inline void NthDerivativeCoder::Reset() {
  for (int i = 0; i < n_; ++i)
    memory_[i] = 0;
  m_ = 0;
}

#endif  // S2_UTIL_CODING_NTH_DERIVATIVE_H_
