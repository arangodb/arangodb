#include <string>
#include "WasmCommon.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "Basics/ResultT.h"

using namespace arangodb::wasm;
using namespace arangodb::velocypack;

void arangodb::wasm::toVelocyPack(WasmFunction const& wasmFunction,
                                  VPackBuilder& builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("name", VPackValue(wasmFunction.name));
  builder.add("code", VPackValue(wasmFunction.code));
  builder.add("isDeterministic", VPackValue(wasmFunction.isDeterministic));
}

auto WasmFunction::fromVelocyPack(Slice slice) -> ResultT<WasmFunction> {
  auto wasmFunction = WasmFunction{};
  if (!slice.hasKey("name")) {
    return ResultT<WasmFunction>::error(TRI_ERROR_BAD_PARAMETER, "Field name is missing");
  }
  if (auto name = slice.get("name"); name.isString()) {
      wasmFunction.name = name.copyString();
  } else {
    return ResultT<WasmFunction>::error(TRI_ERROR_BAD_PARAMETER, "Field name should be a string");
  } 
  if (!slice.hasKey("code")) {
    return ResultT<WasmFunction>::error(TRI_ERROR_BAD_PARAMETER, "Field code is missing");
  }
  if (auto code = slice.get("code"); code.isString()) {
      wasmFunction.code = code.copyString();
  } else {
    return ResultT<WasmFunction>::error(TRI_ERROR_BAD_PARAMETER, "Field code should be a string");
  }
  // TODO decide if isDeterministic should have default value
  if (!slice.hasKey("isDeterministic")) {
    wasmFunction.isDeterministic = false;
  } else {
    if (auto isDeterministic = slice.get("isDeterministic"); isDeterministic.isBool()) {
      wasmFunction.isDeterministic = isDeterministic.getBool();
    } else {
      return ResultT<WasmFunction>::error(TRI_ERROR_BAD_PARAMETER, "Field code should be a bool");
    }
  }
  return ResultT<WasmFunction>(wasmFunction);
}
