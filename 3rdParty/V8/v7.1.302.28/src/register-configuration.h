// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGISTER_CONFIGURATION_H_
#define V8_REGISTER_CONFIGURATION_H_

#include "src/base/macros.h"
#include "src/globals.h"
#include "src/machine-type.h"
#include "src/reglist.h"

namespace v8 {
namespace internal {

// An architecture independent representation of the sets of registers available
// for instruction creation.
class V8_EXPORT_PRIVATE RegisterConfiguration {
 public:
  enum AliasingKind {
    // Registers alias a single register of every other size (e.g. Intel).
    OVERLAP,
    // Registers alias two registers of the next smaller size (e.g. ARM).
    COMBINE
  };

  // Architecture independent maxes.
  static const int kMaxGeneralRegisters = 32;
  static const int kMaxFPRegisters = 32;

  // Default RegisterConfigurations for the target architecture.
  static const RegisterConfiguration* Default();

  // Register configuration with reserved masking register.
  static const RegisterConfiguration* Poisoning();

  // Register configuration with reserved root register on ia32.
  static const RegisterConfiguration* PreserveRootIA32();

  static const RegisterConfiguration* RestrictGeneralRegisters(
      RegList registers);

  RegisterConfiguration(int num_general_registers, int num_double_registers,
                        int num_allocatable_general_registers,
                        int num_allocatable_double_registers,
                        const int* allocatable_general_codes,
                        const int* allocatable_double_codes,
                        AliasingKind fp_aliasing_kind,
                        char const* const* general_names,
                        char const* const* float_names,
                        char const* const* double_names,
                        char const* const* simd128_names);

  int num_general_registers() const { return num_general_registers_; }
  int num_float_registers() const { return num_float_registers_; }
  int num_double_registers() const { return num_double_registers_; }
  int num_simd128_registers() const { return num_simd128_registers_; }
  int num_allocatable_general_registers() const {
    return num_allocatable_general_registers_;
  }
  int num_allocatable_float_registers() const {
    return num_allocatable_float_registers_;
  }
  int num_allocatable_double_registers() const {
    return num_allocatable_double_registers_;
  }
  int num_allocatable_simd128_registers() const {
    return num_allocatable_simd128_registers_;
  }
  AliasingKind fp_aliasing_kind() const { return fp_aliasing_kind_; }
  int32_t allocatable_general_codes_mask() const {
    return allocatable_general_codes_mask_;
  }
  int32_t allocatable_double_codes_mask() const {
    return allocatable_double_codes_mask_;
  }
  int32_t allocatable_float_codes_mask() const {
    return allocatable_float_codes_mask_;
  }
  int GetAllocatableGeneralCode(int index) const {
    DCHECK(index >= 0 && index < num_allocatable_general_registers());
    return allocatable_general_codes_[index];
  }
  bool IsAllocatableGeneralCode(int index) const {
    return ((1 << index) & allocatable_general_codes_mask_) != 0;
  }
  int GetAllocatableFloatCode(int index) const {
    DCHECK(index >= 0 && index < num_allocatable_float_registers());
    return allocatable_float_codes_[index];
  }
  bool IsAllocatableFloatCode(int index) const {
    return ((1 << index) & allocatable_float_codes_mask_) != 0;
  }
  int GetAllocatableDoubleCode(int index) const {
    DCHECK(index >= 0 && index < num_allocatable_double_registers());
    return allocatable_double_codes_[index];
  }
  bool IsAllocatableDoubleCode(int index) const {
    return ((1 << index) & allocatable_double_codes_mask_) != 0;
  }
  int GetAllocatableSimd128Code(int index) const {
    DCHECK(index >= 0 && index < num_allocatable_simd128_registers());
    return allocatable_simd128_codes_[index];
  }
  bool IsAllocatableSimd128Code(int index) const {
    return ((1 << index) & allocatable_simd128_codes_mask_) != 0;
  }
  const char* GetGeneralOrSpecialRegisterName(int code) const;
  const char* GetGeneralRegisterName(int code) const {
    DCHECK_LT(code, num_general_registers_);
    return general_register_names_[code];
  }
  const char* GetFloatRegisterName(int code) const {
    return float_register_names_[code];
  }
  const char* GetDoubleRegisterName(int code) const {
    return double_register_names_[code];
  }
  const char* GetSimd128RegisterName(int code) const {
    return simd128_register_names_[code];
  }
  const int* allocatable_general_codes() const {
    return allocatable_general_codes_;
  }
  const int* allocatable_float_codes() const {
    return allocatable_float_codes_;
  }
  const int* allocatable_double_codes() const {
    return allocatable_double_codes_;
  }
  const int* allocatable_simd128_codes() const {
    return allocatable_simd128_codes_;
  }

  // Aliasing calculations for floating point registers, when fp_aliasing_kind()
  // is COMBINE. Currently only implemented for kFloat32, kFloat64, or kSimd128
  // reps. Returns the number of aliases, and if > 0, alias_base_index is set to
  // the index of the first alias.
  int GetAliases(MachineRepresentation rep, int index,
                 MachineRepresentation other_rep, int* alias_base_index) const;
  // Returns a value indicating whether two registers alias each other, when
  // fp_aliasing_kind() is COMBINE. Currently implemented for kFloat32,
  // kFloat64, or kSimd128 reps.
  bool AreAliases(MachineRepresentation rep, int index,
                  MachineRepresentation other_rep, int other_index) const;

  virtual ~RegisterConfiguration() = default;

 private:
  const int num_general_registers_;
  int num_float_registers_;
  const int num_double_registers_;
  int num_simd128_registers_;
  int num_allocatable_general_registers_;
  int num_allocatable_float_registers_;
  int num_allocatable_double_registers_;
  int num_allocatable_simd128_registers_;
  int32_t allocatable_general_codes_mask_;
  int32_t allocatable_float_codes_mask_;
  int32_t allocatable_double_codes_mask_;
  int32_t allocatable_simd128_codes_mask_;
  const int* allocatable_general_codes_;
  int allocatable_float_codes_[kMaxFPRegisters];
  const int* allocatable_double_codes_;
  int allocatable_simd128_codes_[kMaxFPRegisters];
  AliasingKind fp_aliasing_kind_;
  char const* const* general_register_names_;
  char const* const* float_register_names_;
  char const* const* double_register_names_;
  char const* const* simd128_register_names_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGISTER_CONFIGURATION_H_
