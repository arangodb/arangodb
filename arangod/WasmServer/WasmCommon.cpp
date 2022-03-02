#include <string>
#include "WasmCommon.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

using namespace arangodb::wasm;
using namespace arangodb::velocypack;

void arangodb::wasm::toVelocyPack(WasmFunction const& wasmFunction,
                                  VPackBuilder& builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("name", VPackValue(wasmFunction.name));
  builder.add("code", VPackValue(wasmFunction.code));
  builder.add("isDeterministic", VPackValue(wasmFunction.isDeterministic));
}

auto WasmFunction::fromVelocyPack(Slice slice) -> WasmFunction {
  auto wasmFunction = WasmFunction{};
  if (auto name = slice.get("name"); !name.isNone()) {
    if (name.isString()) {
      wasmFunction.name = name.copyString();
    }
  }
  if (auto code = slice.get("code"); !code.isNone()) {
    if (code.isString()) {
      wasmFunction.code = code.copyString();
    }
  }
  if (auto isDeterministic = slice.get("isDeterministic");
      !isDeterministic.isNone()) {
    if (isDeterministic.isBool()) {
      wasmFunction.isDeterministic = isDeterministic.getBool();
    }
  }
  return wasmFunction;
}
