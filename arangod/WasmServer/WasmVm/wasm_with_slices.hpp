#pragma once

#include "Basics/ResultT.h"
#include "velocypack/Slice.h"
#include "wasm3_interface.hpp"

namespace arangodb::wasm_interface {

auto call_function(WasmVm& vm, const char* function_name,
                   arangodb::velocypack::Slice const& input)
    -> ResultT<arangodb::velocypack::Slice>;

}  // namespace arangodb::wasm_interface
