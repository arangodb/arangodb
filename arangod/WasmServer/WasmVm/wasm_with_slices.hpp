#pragma once

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "velocypack/Slice.h"
#include "wasm3_interface.hpp"

namespace arangodb::wasm_interface {

auto copy_to_wasm(WasmVm& vm, arangodb::velocypack::Slice input)
    -> ResultT<WasmPtr>;
auto deallocate_in_wasm(WasmVm& vm, WasmPtr ptr) -> Result;
auto call_function(WasmVm& vm, const char* function_name,
                   arangodb::velocypack::Slice const& input)
    -> ResultT<arangodb::velocypack::Slice>;

}  // namespace wasm_interface
