#pragma once

#include <cstdint>
#include <concepts>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "fmt/core.h"
#include "wasm3.h"

namespace arangodb::wasm_interface {

struct WasmError {
  std::string message;
  WasmError(std::string message) : message{std::move(message)} {};
  friend std::ostream& operator<<(std::ostream& out, const WasmError& error) {
    return out << "Error: " << error.message;
  }
};

// template<typename OkT>
// struct WasmResult : public Result<OkT, WasmError> {};

using WasmPtr = uint32_t;

template<typename T>
// TODO should be i32 || i64 || f32 || f64
concept WasmType = std::same_as<T, uint64_t> || std::same_as<T, WasmPtr>;

struct WasmVm {
 private:
  IM3Environment environment;
  std::vector<std::vector<uint8_t>> code;
  std::unordered_map<std::string, IM3Runtime> runtimes;
  auto error(std::string const& module, M3Result const& err) -> std::optional<std::string>;

 public:
  WasmVm();
  ~WasmVm();
  template<WasmType Output, WasmType... Input>
  auto call_function(std::string const& name, const char* function_name, Input... input)
      -> ResultT<Output>;
  auto load_module(std::string const& name, std::vector<uint8_t> modulecode) -> Result;
  // TODO return WasmResult
  auto memory_pointer(std::string const&, WasmPtr ptr) -> uint8_t*;

  // TODO add function to link a function to module
};

}  // namespace arangodb::wasm_interface

template<arangodb::wasm_interface::WasmType Output,
         arangodb::wasm_interface::WasmType... Input>
auto arangodb::wasm_interface::WasmVm::call_function(std::string const& name, const char* function_name,
                                                     Input... input)
    -> ResultT<Output> {
  IM3Function function;

  // find runtime
  auto this_runtime = runtimes.find(name);
  if (this_runtime == runtimes.end()) {
    return ResultT<Output>::error(TRI_ERROR_WASM_EXECUTION_ERROR, fmt::format("Runtime for module {} not found", name));
  }
  
  // find function
  auto e = error(name, m3_FindFunction(&function, this_runtime->second, function_name));
  if (e.has_value()) {
    return ResultT<Output>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Function {} not found in {}: {}", function_name, name, e.value()));
  }

  // call function
  const void* input_ptrs[] = {reinterpret_cast<const void*>(&input)...};
  e = error(name, m3_Call(function, sizeof...(input), input_ptrs));
  if (e.has_value()) {
    return ResultT<Output>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Cannot call function {} in {}: {}", function_name, name, e.value()));
  }

  // get return value
  Output val;
  const void* ptr = &val;
  e = error(name, m3_GetResults(function, 1, &ptr));
  if (e.has_value()) {
    return ResultT<Output>::error(
        TRI_ERROR_WASM_EXECUTION_ERROR,
        fmt::format("Cannot retrieve output for function {}: {}", function_name,
                    e.value()));
  }
  return *(Output*)ptr;
}
