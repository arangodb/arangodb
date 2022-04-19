#include "wasm3_interface.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

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

auto WasmVm::error(M3Result const& err) -> std::optional<std::string_view> {
  if (err != m3Err_none) {
    return err;
  }
  return {};
}

auto WasmVm::load_module(uint8_t* data, size_t size) -> Result {
  IM3Module module;

  auto e = error(m3_ParseModule(environment, &module, data, size));
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
