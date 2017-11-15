// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SSE_INSTR_H_
#define V8_SSE_INSTR_H_

#define SSE2_INSTRUCTION_LIST(V) \
  V(packsswb, 66, 0F, 63)        \
  V(packssdw, 66, 0F, 6B)        \
  V(packuswb, 66, 0F, 67)        \
  V(paddb, 66, 0F, FC)           \
  V(paddw, 66, 0F, FD)           \
  V(paddd, 66, 0F, FE)           \
  V(paddsb, 66, 0F, EC)          \
  V(paddsw, 66, 0F, ED)          \
  V(paddusb, 66, 0F, DC)         \
  V(paddusw, 66, 0F, DD)         \
  V(pcmpeqb, 66, 0F, 74)         \
  V(pcmpeqw, 66, 0F, 75)         \
  V(pcmpeqd, 66, 0F, 76)         \
  V(pcmpgtb, 66, 0F, 64)         \
  V(pcmpgtw, 66, 0F, 65)         \
  V(pcmpgtd, 66, 0F, 66)         \
  V(pmaxsw, 66, 0F, EE)          \
  V(pmaxub, 66, 0F, DE)          \
  V(pminsw, 66, 0F, EA)          \
  V(pminub, 66, 0F, DA)          \
  V(pmullw, 66, 0F, D5)          \
  V(pmuludq, 66, 0F, F4)         \
  V(psllw, 66, 0F, F1)           \
  V(pslld, 66, 0F, F2)           \
  V(psraw, 66, 0F, E1)           \
  V(psrad, 66, 0F, E2)           \
  V(psrlw, 66, 0F, D1)           \
  V(psrld, 66, 0F, D2)           \
  V(psubb, 66, 0F, F8)           \
  V(psubw, 66, 0F, F9)           \
  V(psubd, 66, 0F, FA)           \
  V(psubsb, 66, 0F, E8)          \
  V(psubsw, 66, 0F, E9)          \
  V(psubusb, 66, 0F, D8)         \
  V(psubusw, 66, 0F, D9)         \
  V(pxor, 66, 0F, EF)            \
  V(cvtps2dq, 66, 0F, 5B)

#define SSSE3_INSTRUCTION_LIST(V) \
  V(pabsb, 66, 0F, 38, 1C)        \
  V(pabsw, 66, 0F, 38, 1D)        \
  V(pabsd, 66, 0F, 38, 1E)        \
  V(pshufb, 66, 0F, 38, 00)       \
  V(psignb, 66, 0F, 38, 08)       \
  V(psignw, 66, 0F, 38, 09)       \
  V(psignd, 66, 0F, 38, 0A)

#define SSE4_INSTRUCTION_LIST(V) \
  V(packusdw, 66, 0F, 38, 2B)    \
  V(pminsb, 66, 0F, 38, 38)      \
  V(pminsd, 66, 0F, 38, 39)      \
  V(pminuw, 66, 0F, 38, 3A)      \
  V(pminud, 66, 0F, 38, 3B)      \
  V(pmaxsb, 66, 0F, 38, 3C)      \
  V(pmaxsd, 66, 0F, 38, 3D)      \
  V(pmaxuw, 66, 0F, 38, 3E)      \
  V(pmaxud, 66, 0F, 38, 3F)      \
  V(pmulld, 66, 0F, 38, 40)      \
  V(ptest, 66, 0F, 38, 17)

#endif  // V8_SSE_INSTR_H_
