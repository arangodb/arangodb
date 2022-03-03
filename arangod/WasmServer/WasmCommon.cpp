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

  if (auto nameslice = requiredStringSliceField("name", slice);
      nameslice.ok()) {
    wasmFunction.name = nameslice.get().copyString();
  } else {
    return ResultT<WasmFunction>::error(nameslice.errorNumber(),
                                        nameslice.errorMessage());
  }

  if (auto codeslice = requiredStringSliceField("code", slice);
      codeslice.ok()) {
    wasmFunction.code = codeslice.get().copyString();
  } else {
    return ResultT<WasmFunction>::error(codeslice.errorNumber(),
                                        codeslice.errorMessage());
  }

  if (auto isDeterministic = deterministicField(slice); isDeterministic.ok()) {
    wasmFunction.isDeterministic = isDeterministic.get();
  } else {
    return ResultT<WasmFunction>::error(isDeterministic.errorNumber(),
                                        isDeterministic.errorMessage());
  }

  return ResultT<WasmFunction>(wasmFunction);
}

auto arangodb::wasm::requiredStringSliceField(std::string_view fieldName,
                                              Slice slice)
    -> arangodb::ResultT<Slice> {
  if (!slice.hasKey(fieldName)) {
    return arangodb::ResultT<Slice>::error(TRI_ERROR_BAD_PARAMETER,
                                           "Field name is missing");
  }
  if (auto name = slice.get(fieldName); name.isString()) {
    return name;
  } else {
    return arangodb::ResultT<Slice>::error(TRI_ERROR_BAD_PARAMETER,
                                           "Field name should be a string");
  }
}

auto arangodb::wasm::deterministicField(Slice slice)
    -> arangodb::ResultT<bool> {
  if (!slice.hasKey("isDeterministic")) {
    return false;
  }
  if (auto isDeterministic = slice.get("isDeterministic");
      isDeterministic.isBool()) {
    return isDeterministic.getBool();
  } else {
    return arangodb::ResultT<bool>::error(TRI_ERROR_BAD_PARAMETER,
                                          "Field code should be a bool");
  }
}
