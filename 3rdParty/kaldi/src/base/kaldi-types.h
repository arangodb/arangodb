// base/kaldi-types.h

// Copyright 2009-2011  Microsoft Corporation;  Saarland University;
//                      Jan Silovsky;  Yanmin Qian

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#ifndef KALDI_BASE_KALDI_TYPES_H_
#define KALDI_BASE_KALDI_TYPES_H_ 1

namespace kaldi {
// TYPEDEFS ..................................................................
#if (KALDI_DOUBLEPRECISION != 0)
typedef double  BaseFloat;
#else
typedef float   BaseFloat;
#endif
}

#ifdef _WIN32
#include <basetsd.h>
using ssize_t = SSIZE_T;
#endif

// we can do this a different way if some platform
// we find in the future lacks stdint.h
#include <cstdint>

namespace kaldi {
  using int16 = std::int16_t;
  using int32 = std::int32_t;
  using int64 = std::int64_t;

  using uint16 = std::uint16_t;
  using uint32 = std::uint32_t;
  using uint64 = std::uint64_t;

  using float32 = float;
  using double64 = double;
}  // end namespace kaldi

#endif  // KALDI_BASE_KALDI_TYPES_H_
