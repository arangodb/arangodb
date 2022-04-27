#include "wasm3_interface.hpp"

#include <cstdint>
#include <cstring>
#include <string>

#include "Basics/Result.h"
#include "fmt/core.h"
#include "wasm3.h"

using namespace arangodb::wasm_interface;

WasmVm::WasmVm() {
  // TODO environment could be NULL
  environment = m3_NewEnvironment();
}

WasmVm::~WasmVm() {
  m3_FreeEnvironment(environment);
  for (auto rt: runtimes) {
    m3_FreeRuntime(rt.second);
  }
}

auto errorInfos(IM3Runtime const& runtime) -> std::string {
    // there will be an error info if d_m3VerboseErrorMessages = 1
    M3ErrorInfo info;
    m3_GetErrorInfo(runtime, &info);
    if (info.result == m3Err_none) {
      return "";
    }
    return fmt::format(" - {}, {}:{}",
				      info.message, info.file, info.line);
}

auto WasmVm::error(std::string const& module, M3Result const& err) -> std::optional<std::string> {
  if (err != m3Err_none) {
    auto runtime = runtimes.find(module);
    if (runtime == runtimes.end()) {
      return fmt::format("wasm3 error: {}", err);
    }
    return fmt::format("wasm3 error: {}{}", err, errorInfos(runtime->second));
  }
  return {};
}

auto WasmVm::load_module(std::string const& name, std::vector<uint8_t> modulecode) -> Result {
  code.emplace_back(std::move(modulecode));

  IM3Module module;
  auto e = error(name, m3_ParseModule(environment, &module, code.back().data(),
                                code.back().size()));
  if (e.has_value()) {
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR,
                  fmt::format("Cannot parse module: {}", e.value())};
  }

  runtimes.emplace(name, m3_NewRuntime(environment, 65536, NULL)); 
  auto this_runtime = runtimes.find(name);
  if (this_runtime == runtimes.end()) {
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR, fmt::format("Runtime for module {} was not created successfully", name)};
  }
  e = error(name, m3_LoadModule(this_runtime->second, module));
  if (e.has_value()) {
    // TODO free runtime and delete from map
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR,
                  fmt::format("Cannot load module: {}", e.value())};
  }
  return {};
}

auto WasmVm::memory_pointer(std::string const& module_name, WasmPtr ptr) -> uint8_t* {
  auto this_runtime = runtimes.find(module_name);
  // TODO error handling if runtime cannot be found
  uint8_t* wasm_memory = m3_GetMemory(this_runtime->second, nullptr, 0);
  return wasm_memory + ptr;
}
