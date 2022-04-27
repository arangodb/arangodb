#include "wasm_with_slices.hpp"
#include <cstdint>
#include <cstring>

#include "Basics/ResultT.h"
#include "velocypack/Slice.h"
#include "wasm3_interface.hpp"
#include "fmt/core.h"

using namespace arangodb;
using namespace arangodb::wasm_interface;
using namespace arangodb::velocypack;

auto copy_to_wasm(WasmVm& vm, std::string const& module_name, Slice input) -> ResultT<WasmPtr> {
  auto ptr = vm.call_function<WasmPtr>("allocate", "allocate",
                                       static_cast<uint32_t>(input.byteSize()));
  if (ptr.fail()) {
    return ResultT<WasmPtr>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format(
            "Unable to allocate memory in WebAssembly VM to copy input: {}",
            ptr.errorMessage()));
  }
  memcpy(vm.memory_pointer(module_name, ptr.get()), input.start(), input.byteSize());
  return ptr;
}

auto deallocate_in_wasm(WasmVm& vm, WasmPtr ptr) -> Result {
  if (auto res = vm.call_function<uint32_t>("allocate", "deallocate", ptr); res.fail()) {
    return Result{
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Unable to deallocate memory in WebAssembly VM: {}",
                    res.errorMessage())};
  }
  return {};
}

auto copy_from_wasm(uint8_t* ptr) -> ResultT<Slice> {
  auto slice = Slice(ptr);

  auto cannotParse = ResultT<Slice>::error(TRI_ERROR_WASM_EXECUTION_ERROR,
                                           "Cannot read WASM result");
  if (!slice.isObject()) {
    return cannotParse;
  }
  if (slice.keyAt(0).isEqualString("Ok")) {
    auto ok = slice.valueAt(0);
    auto newSlice = new uint8_t[ok.byteSize()];
    std::memcpy(newSlice, ok.start(), ok.byteSize());
    return Slice(newSlice);
  } else if (slice.keyAt(0).isEqualString("Error")) {
    if (!slice.valueAt(0).isString()) {
      return cannotParse;
    }
    return ResultT<Slice>::error(TRI_ERROR_WASM_EXECUTION_ERROR,
                                 slice.valueAt(0).copyString());
  }
  return cannotParse;
}

auto arangodb::wasm_interface::call_function(WasmVm& vm,
					     std::string const& module_name,
                                             const char* function_name,
                                             Slice const& input)
    -> ResultT<Slice> {
  auto inPtr = copy_to_wasm(vm, module_name, input);

  if (inPtr.fail()) {
   // TODO dealloc in
    return ResultT<Slice>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Function {}: {}", function_name, inPtr.errorMessage()));
  }

  auto outPtr = vm.call_function<WasmPtr>(module_name, function_name, inPtr.get());
  if (outPtr.fail()) {
    // TODO dealloc in
    return ResultT<Slice>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Function {}: {}", function_name, outPtr.errorMessage()));
  }

  auto wasmResult = reinterpret_cast<uint8_t*>(vm.memory_pointer(module_name, outPtr.get()));
  auto output = copy_from_wasm(wasmResult);
  if (output.fail()) {
    // TODO dealloc in and out
    return ResultT<Slice>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Function {}: {}", function_name, output.errorMessage()));
  }

  if (auto res = deallocate_in_wasm(vm, outPtr.get()); res.fail()) {
    // TODO dealloc in
    return ResultT<Slice>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Function {} output: {}", function_name,
                    res.errorMessage()));
  }
  if (auto res = deallocate_in_wasm(vm, inPtr.get()); res.fail()) {
    return ResultT<Slice>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Function {} input: {}", function_name,
                    res.errorMessage()));
  }

  return output;
}
