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
  runtime = m3_NewRuntime(environment, 65536, NULL);
}

WasmVm::~WasmVm() {
  m3_FreeRuntime(runtime);
  m3_FreeEnvironment(environment);
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
auto WasmVm::error(M3Result const& err) -> std::optional<std::string> {
  if (err != m3Err_none) {
    return fmt::format("wasm3 error: {}{}", err, errorInfos(runtime));
  }
  return {};
}

auto WasmVm::load_module(std::vector<uint8_t> modulecode) -> Result {
  code.emplace_back(std::move(modulecode));

  IM3Module module;
  auto e = error(m3_ParseModule(environment, &module, code.back().data(),
                                code.back().size()));
  if (e.has_value()) {
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR,
                  fmt::format("Cannot parse module: {}", e.value())};
  }
  e = error(m3_LoadModule(runtime, module));
  if (e.has_value()) {
    return Result{TRI_ERROR_WASM_EXECUTION_ERROR,
                  fmt::format("Cannot load module: {}", e.value())};
  }
  return {};
}

auto WasmVm::memory_pointer(WasmPtr ptr) -> uint8_t* {
  uint8_t* wasm_memory = m3_GetMemory(runtime, nullptr, 0);
  return wasm_memory + ptr;
}
