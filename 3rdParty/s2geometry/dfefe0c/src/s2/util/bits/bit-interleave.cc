// Copyright 2009 Google Inc. All Rights Reserved.
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

// Author: jyrki@google.com (Jyrki Alakuijala)
//
// Performance notes:
//   2009-01-16: hendrie@google.com microbenchmarked InterleaveUint32 against
//     two table-free implementations from "Hacker's Delight" section 7-2.
//     This implementation was substantially faster: 8 ns vs 17 ns, even
//     without subtracting the time taken in the test harness itself.
//     Test environment: workstation w/ 2.4 GHz Core 2 Duo, compiler target
//       gcc-4.3.1-glibc-2.3.6-grte-k8.
//     TODO(user): Inlining InterleaveUint32 yields a measurable speedup (5
//     ns vs. 8 ns). Consider cost/benefit of moving implementations inline.

#include "s2/util/bits/bit-interleave.h"

#include "s2/base/integral_types.h"

namespace util_bits {

static const uint16 kInterleaveLut[256] = {
  0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015,
  0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055,
  0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115,
  0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155,
  0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415,
  0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455,
  0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515,
  0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555,

  0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015,
  0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
  0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115,
  0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
  0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415,
  0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
  0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515,
  0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,

  0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015,
  0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
  0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115,
  0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
  0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415,
  0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
  0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515,
  0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,

  0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015,
  0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
  0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115,
  0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
  0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415,
  0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
  0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515,
  0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555,
};

uint16 InterleaveUint8(const uint8 val0, const uint8 val1) {
  return kInterleaveLut[val0] | (kInterleaveLut[val1] << 1);
}

uint32 InterleaveUint16(const uint16 val0, const uint16 val1) {
  return kInterleaveLut[val0 & 0xff] |
      (kInterleaveLut[val0 >> 8] << 16) |
      (kInterleaveLut[val1 & 0xff] << 1) |
      (kInterleaveLut[val1 >> 8] << 17);
}

uint64 InterleaveUint32(const uint32 val0, const uint32 val1) {
  return
      (static_cast<uint64>(kInterleaveLut[val0 & 0xff])) |
      (static_cast<uint64>(kInterleaveLut[(val0 >> 8) & 0xff]) << 16) |
      (static_cast<uint64>(kInterleaveLut[(val0 >> 16) & 0xff]) << 32) |
      (static_cast<uint64>(kInterleaveLut[val0 >> 24]) << 48) |
      (static_cast<uint64>(kInterleaveLut[val1 & 0xff]) << 1) |
      (static_cast<uint64>(kInterleaveLut[(val1 >> 8) & 0xff]) << 17) |
      (static_cast<uint64>(kInterleaveLut[(val1 >> 16) & 0xff]) << 33) |
      (static_cast<uint64>(kInterleaveLut[val1 >> 24]) << 49);
}

// The lookup table below can convert a sequence of interleaved 8 bits into
// non-interleaved 4 bits. The table can convert both odd and even bits at the
// same time, and lut[x & 0x55] converts the even bits (bits 0, 2, 4 and 6),
// while lut[x & 0xaa] converts the odd bits (bits 1, 3, 5 and 7).
//
// The lookup table below was generated using the following python code:
//
// def deinterleave(bits):
//   if bits == 0: return 0
//   if bits < 4: return 1
//   return deinterleave(bits / 4) * 2 + deinterleave(bits & 3)
//
// for i in range(256): print "0x%x," % deinterleave(i),
//
static const uint8 kDeinterleaveLut[256] = {
  0x0, 0x1, 0x1, 0x1, 0x2, 0x3, 0x3, 0x3,
  0x2, 0x3, 0x3, 0x3, 0x2, 0x3, 0x3, 0x3,
  0x4, 0x5, 0x5, 0x5, 0x6, 0x7, 0x7, 0x7,
  0x6, 0x7, 0x7, 0x7, 0x6, 0x7, 0x7, 0x7,
  0x4, 0x5, 0x5, 0x5, 0x6, 0x7, 0x7, 0x7,
  0x6, 0x7, 0x7, 0x7, 0x6, 0x7, 0x7, 0x7,
  0x4, 0x5, 0x5, 0x5, 0x6, 0x7, 0x7, 0x7,
  0x6, 0x7, 0x7, 0x7, 0x6, 0x7, 0x7, 0x7,

  0x8, 0x9, 0x9, 0x9, 0xa, 0xb, 0xb, 0xb,
  0xa, 0xb, 0xb, 0xb, 0xa, 0xb, 0xb, 0xb,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,

  0x8, 0x9, 0x9, 0x9, 0xa, 0xb, 0xb, 0xb,
  0xa, 0xb, 0xb, 0xb, 0xa, 0xb, 0xb, 0xb,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,

  0x8, 0x9, 0x9, 0x9, 0xa, 0xb, 0xb, 0xb,
  0xa, 0xb, 0xb, 0xb, 0xa, 0xb, 0xb, 0xb,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,
  0xc, 0xd, 0xd, 0xd, 0xe, 0xf, 0xf, 0xf,
  0xe, 0xf, 0xf, 0xf, 0xe, 0xf, 0xf, 0xf,
};

void DeinterleaveUint8(uint16 val, uint8 *val0, uint8 *val1) {
  *val0 = ((kDeinterleaveLut[val & 0x55]) |
           (kDeinterleaveLut[(val >> 8) & 0x55] << 4));
  *val1 = ((kDeinterleaveLut[val & 0xaa]) |
           (kDeinterleaveLut[(val >> 8) & 0xaa] << 4));
}

void DeinterleaveUint16(uint32 code, uint16 *val0, uint16 *val1) {
  *val0 = ((kDeinterleaveLut[code & 0x55]) |
           (kDeinterleaveLut[(code >> 8) & 0x55] << 4) |
           (kDeinterleaveLut[(code >> 16) & 0x55] << 8) |
           (kDeinterleaveLut[(code >> 24) & 0x55] << 12));
  *val1 = ((kDeinterleaveLut[code & 0xaa]) |
           (kDeinterleaveLut[(code >> 8) & 0xaa] << 4) |
           (kDeinterleaveLut[(code >> 16) & 0xaa] << 8) |
           (kDeinterleaveLut[(code >> 24) & 0xaa] << 12));
}

void DeinterleaveUint32(uint64 code, uint32 *val0, uint32 *val1) {
  *val0 = ((kDeinterleaveLut[code & 0x55]) |
           (kDeinterleaveLut[(code >> 8) & 0x55] << 4) |
           (kDeinterleaveLut[(code >> 16) & 0x55] << 8) |
           (kDeinterleaveLut[(code >> 24) & 0x55] << 12) |
           (kDeinterleaveLut[(code >> 32) & 0x55] << 16) |
           (kDeinterleaveLut[(code >> 40) & 0x55] << 20) |
           (kDeinterleaveLut[(code >> 48) & 0x55] << 24) |
           (kDeinterleaveLut[(code >> 56) & 0x55] << 28));
  *val1 = ((kDeinterleaveLut[code & 0xaa]) |
           (kDeinterleaveLut[(code >> 8) & 0xaa] << 4) |
           (kDeinterleaveLut[(code >> 16) & 0xaa] << 8) |
           (kDeinterleaveLut[(code >> 24) & 0xaa] << 12) |
           (kDeinterleaveLut[(code >> 32) & 0xaa] << 16) |
           (kDeinterleaveLut[(code >> 40) & 0xaa] << 20) |
           (kDeinterleaveLut[(code >> 48) & 0xaa] << 24) |
           (kDeinterleaveLut[(code >> 56) & 0xaa] << 28));
}

// Derivation of the multiplication based interleave algorithm:
// 1. Original value, bit positions shown:
//    x = --------------------------------------------------------87654321
// 2. Replicate the byte and select the bits we want:
//    * 0b0000000100000001000000010000000100000001000000010000000100000001
//      0x   0   1   0   1   0   1   0   1   0   1   0   1   0   1   0   1
//   =>   8765432187654321876543218765432187654321876543218765432187654321
//    & 0b0000000000000000000000001100000000001100000000000011000000000011
//      0x   0   0   0   0   0   0   C   0   0   C   0   0   3   0   0   3
//   =>   ------------------------87----------43------------65----------21
// 3. Use multiplication to perform additions at different offsets:
//    * 0b0000000000000000000000000000000000000000010100000000000000000101
//      0x   0   0   0   0   0   0   0   0   0   0   5   0   0   0   0   5
//   =>
//        ------------------------87----------43------------65----------21
//      + ----------------------87----------43------------65----------21
//      + ----87----------43------------65----------21
//      + --87----------43------------65----------21
//   =>   --8787--------4343----8787--6565--4343--2121----6565--------2121
// 4. Notice the bits never collide.  Select the bits we want.
//    & 0b0000000000000000000000100100100100100100100100000000000000000000
//      0x   0   0   0   0   0   2   4   9   2   4   9   0   0   0   0   0
//   =>   ----------------------8--7--6--5--4--3--2--1--------------------
// 5. To produce the final result, we bitwise OR the 3 split integers and shift
//    back to the LSB.  To save instructions, we use template<N> to have steps
//    3 and 4 produce results shifted over N bits.
// Benchmark                      Time(ns)    CPU(ns) Iterations
// -------------------------------------------------------------
// BM_3_InterleaveUint8                  5          5  141967960
// BM_3_ReferenceBitInterleave3         58         58   10000000
// BM_3_InterleaveUint8_NoTemplate      11         11   61082024
template<int kShift>
static uint64 SplitFor3(uint8 x) {
  return
      ((((x * 0x0101010101010101ULL)
            & 0x000000C00C003003ULL)
            * (0x0000000000500005ULL << kShift))
            & (0x0000024924900000ULL << kShift));
}

uint32 InterleaveUint8(uint8 val0, uint8 val1, uint8 val2) {
  return static_cast<uint32>(
      (SplitFor3<0>(val0) | SplitFor3<1>(val1) | SplitFor3<2>(val2)) >> 20);
}

// Multiplication based de-interleave algorithm:
// 1. Original value, bit positions shown:
//        --------xx8xx7xx6xx5xx4xx3xx2xx1
//    & 0x   0   0   2   4   9   2   4   9
//   =>   ----------8--7--6--5--4--3--2--1
// 2. Use multiplication to perform additions at different offsets:
//    * 0b00000000000000000000000000010101
//      0x   0   0   0   0   0   0   1   5
//   =>
//        ----------8--7--6--5--4--3--2--1
//     +  --------8--7--6--5--4--3--2--1
//     +  ------8--7--6--5--4--3--2--1
//   =>   ------8-8787676565454343232121-1
// 3. Select the bits we need
//    & 0b00000000001110000001110000001100
//      0x   0   0   3   8   1   C   0   C
//   =>   ----------876------543------21--
// 4. Multiply again to make the additions we need
//    * 0b00000000000000000001000001000001
//      0x   0   0   0   0   1   0   4   1
//   =>
//        ----------876------543------21--
//     +  ----876------543------21--
//     +  6------543------21--
//   =>   ----------87654321--------------
// 5. Shift down so it lives at the lower 8 bits.
// Benchmark                         Time(ns)    CPU(ns) Iterations
// ----------------------------------------------------------------
// BM_3_DeinterleaveUint8                   8          8   88136788
// BM_3_DeinterleaveUint8_Using_Template   10         10   67385445
// BM_3_DeinterleaveUint8_Uint64_Param     10         10   70838731
// BM_3_ReferenceDeinterleaveUint8         79         79    8712211
static inline uint8 UnsplitFor3(uint32 x) {
  return ((((x & 0x00249249U)
               * 0x00000015U)
               & 0x00381C0CU)
               * 0x00001041U) >> 14;
}

void DeinterleaveUint8(uint32 x, uint8* a, uint8* b, uint8* c) {
  *a = UnsplitFor3(x);
  *b = UnsplitFor3(x >> 1);
  *c = UnsplitFor3(x >> 2);
}

}  // namespace util_bits
