// Copyright 2022 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/crc/internal/cpu_detect.h"

#include <cstdint>
#include <string>

#include "absl/base/config.h"

#if defined(__aarch64__) && defined(__linux__)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace crc_internal {

#if defined(__x86_64__)

// Inline cpuid instruction. %rbx is occasionally used to address stack
// variables in presence of dynamic allocas. Preserve the %rbx register via
// %rdi to work around a clang bug https://bugs.llvm.org/show_bug.cgi?id=17907
// (%rbx in an output constraint is not considered a clobbered register).
//
// a_inp and c_inp are the input parameters eax and ecx of the CPUID
// instruction.
// a, b, c, and d contain the contents of eax, ebx, ecx, and edx as returned by
// the CPUID instruction
#define ABSL_INTERNAL_GETCPUID(a, b, c, d, a_inp, c_inp) \
  asm("mov %%rbx, %%rdi\n"                               \
      "cpuid\n"                                          \
      "xchg %%rdi, %%rbx\n"                              \
      : "=a"(a), "=D"(b), "=c"(c), "=d"(d)               \
      : "a"(a_inp), "2"(c_inp))

namespace {

enum class Vendor {
  kUnknown,
  kIntel,
  kAmd,
};

Vendor GetVendor() {
  uint32_t eax, ebx, ecx, edx;

  // Get vendor string (issue CPUID with eax = 0)
  ABSL_INTERNAL_GETCPUID(eax, ebx, ecx, edx, 0, 0);
  std::string vendor;
  vendor.append(reinterpret_cast<char*>(&ebx), 4);
  vendor.append(reinterpret_cast<char*>(&edx), 4);
  vendor.append(reinterpret_cast<char*>(&ecx), 4);
  if (vendor == "GenuineIntel") {
    return Vendor::kIntel;
  } else if (vendor == "AuthenticAmd") {
    return Vendor::kAmd;
  } else {
    return Vendor::kUnknown;
  }
}

CpuType GetIntelCpuType() {
  uint32_t eax, ebx, ecx, edx;
  // to get general information and extended features we send eax = 1 and
  // ecx = 0 to cpuid.  The response is returned in eax, ebx, ecx and edx.
  // (See Intel 64 and IA-32 Architectures Software Developer's Manual
  // Volume 2A: Instruction Set Reference, A-M CPUID).
  // https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-vol-2a-manual.html
  ABSL_INTERNAL_GETCPUID(eax, ebx, ecx, edx, 1, 0);

  // Response in eax bits as follows:
  // 0-3 (stepping id)
  // 4-7 (model number),
  // 8-11 (family code),
  // 12-13 (processor type),
  // 16-19 (extended model)
  // 20-27 (extended family)

  int family = (eax >> 8) & 0x0f;
  int model_num = (eax >> 4) & 0x0f;
  int ext_family = (eax >> 20) & 0xff;
  int ext_model_num = (eax >> 16) & 0x0f;

  int brand_id = ebx & 0xff;

  // Process the extended family and model info if necessary
  if (family == 0x0f) {
    family += ext_family;
  }

  if (family == 0x0f || family == 0x6) {
    model_num += (ext_model_num << 4);
  }

  switch (brand_id) {
    case 0:  // no brand ID, so parse CPU family/model
      switch (family) {
        case 6:  // Most PentiumIII processors are in this category
          switch (model_num) {
            case 0x2c:  // Westmere: Gulftown
              return CpuType::kIntelWestmere;
            case 0x2d:  // Sandybridge
              return CpuType::kIntelSandybridge;
            case 0x3e:  // Ivybridge
              return CpuType::kIntelIvybridge;
            case 0x3c:  // Haswell (client)
            case 0x3f:  // Haswell
              return CpuType::kIntelHaswell;
            case 0x4f:  // Broadwell
            case 0x56:  // BroadwellDE
              return CpuType::kIntelBroadwell;
            case 0x55:                 // Skylake Xeon
              if ((eax & 0x0f) < 5) {  // stepping < 5 is skylake
                return CpuType::kIntelSkylakeXeon;
              } else {  // stepping >= 5 is cascadelake
                return CpuType::kIntelCascadelakeXeon;
              }
            case 0x5e:  // Skylake (client)
              return CpuType::kIntelSkylake;
            default:
              return CpuType::kUnknown;
          }
        default:
          return CpuType::kUnknown;
      }
    default:
      return CpuType::kUnknown;
  }
}

CpuType GetAmdCpuType() {
  uint32_t eax, ebx, ecx, edx;
  // to get general information and extended features we send eax = 1 and
  // ecx = 0 to cpuid.  The response is returned in eax, ebx, ecx and edx.
  // (See Intel 64 and IA-32 Architectures Software Developer's Manual
  // Volume 2A: Instruction Set Reference, A-M CPUID).
  ABSL_INTERNAL_GETCPUID(eax, ebx, ecx, edx, 1, 0);

  // Response in eax bits as follows:
  // 0-3 (stepping id)
  // 4-7 (model number),
  // 8-11 (family code),
  // 12-13 (processor type),
  // 16-19 (extended model)
  // 20-27 (extended family)

  int family = (eax >> 8) & 0x0f;
  int model_num = (eax >> 4) & 0x0f;
  int ext_family = (eax >> 20) & 0xff;
  int ext_model_num = (eax >> 16) & 0x0f;

  if (family == 0x0f) {
    family += ext_family;
    model_num += (ext_model_num << 4);
  }

  switch (family) {
    case 0x17:
      switch (model_num) {
        case 0x0:  // Stepping Ax
        case 0x1:  // Stepping Bx
          return CpuType::kAmdNaples;
        case 0x30:  // Stepping Ax
        case 0x31:  // Stepping Bx
          return CpuType::kAmdRome;
        default:
          return CpuType::kUnknown;
      }
      break;
    case 0x19:
      switch (model_num) {
        case 0x1:  // Stepping B0
          return CpuType::kAmdMilan;
        default:
          return CpuType::kUnknown;
      }
      break;
    default:
      return CpuType::kUnknown;
  }
}

}  // namespace

CpuType GetCpuType() {
  switch (GetVendor()) {
    case Vendor::kIntel:
      return GetIntelCpuType();
    case Vendor::kAmd:
      return GetAmdCpuType();
    default:
      return CpuType::kUnknown;
  }
}

bool SupportsArmCRC32PMULL() { return false; }

#elif defined(__aarch64__) && defined(__linux__)

#define ABSL_INTERNAL_AARCH64_ID_REG_READ(id, val) \
  asm("mrs %0, " #id : "=r"(val))

CpuType GetCpuType() {
  // MIDR_EL1 is not visible to EL0, however the access will be emulated by
  // linux if AT_HWCAP has HWCAP_CPUID set.
  //
  // This method will be unreliable on heterogeneous computing systems (ex:
  // big.LITTLE) since the value of MIDR_EL1 will change based on the calling
  // thread.
  uint64_t hwcaps = getauxval(AT_HWCAP);
  if (hwcaps & HWCAP_CPUID) {
    uint64_t midr = 0;
    ABSL_INTERNAL_AARCH64_ID_REG_READ(MIDR_EL1, midr);
    uint32_t implementer = (midr >> 24) & 0xff;
    uint32_t part_number = (midr >> 4) & 0xfff;
    if (implementer == 0x41 && part_number == 0xd0c) {
      return CpuType::kArmNeoverseN1;
    }
  }
  return CpuType::kUnknown;
}

bool SupportsArmCRC32PMULL() {
  uint64_t hwcaps = getauxval(AT_HWCAP);
  return (hwcaps & HWCAP_CRC32) && (hwcaps & HWCAP_PMULL);
}

#else

CpuType GetCpuType() { return CpuType::kUnknown; }

bool SupportsArmCRC32PMULL() { return false; }

#endif

}  // namespace crc_internal
ABSL_NAMESPACE_END
}  // namespace absl
