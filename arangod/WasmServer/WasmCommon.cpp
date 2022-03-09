#include <optional>
#include <regex>
#include <string>
#include <set>
#include "Basics/StringUtils.h"
#include "WasmCommon.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"
#include "velocypack/vpack.h"
#include "Basics/ResultT.h"
#include "Basics/Result.h"
#include <iostream>

using namespace arangodb;
using namespace arangodb::wasm;
using namespace arangodb::velocypack;

void arangodb::wasm::wasmFunction2Velocypack(WasmFunction const& wasmFunction,
                                             VPackBuilder& builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("name", VPackValue(wasmFunction.name));
  builder.add(VPackValue("code"));
  arangodb::wasm::code2Velocypack(wasmFunction.code, builder);
  builder.add("isDeterministic", VPackValue(wasmFunction.isDeterministic));
}

void arangodb::wasm::code2Velocypack(Code const& code, VPackBuilder& builder) {
  auto ab = VPackArrayBuilder(&builder);
  for (auto const& entry : code.bytes) {
    builder.add(VPackValue(entry));
  }
}

auto arangodb::wasm::velocypack2WasmFunction(Slice slice)
    -> ResultT<WasmFunction> {
  std::string functionName = "velocypack2WasmFunction";

  if (auto check =
          arangodb::wasm::checkVelocypack2WasmFunctionIsPossible(slice);
      check.fail()) {
    return ResultT<WasmFunction>::error(
        check.errorNumber(), functionName + std::move(check).errorMessage());
  }

  auto name = arangodb::wasm::velocypack2Name(slice.get("name"));
  if (!name.ok()) {
    return ResultT<WasmFunction>::error(
        name.errorNumber(),
        functionName + ": Field 'name': " + std::move(name).errorMessage());
  }

  auto codeField = arangodb::wasm::velocypack2Code(slice.get("code"));
  if (!codeField.ok()) {
    return ResultT<WasmFunction>::error(
        codeField.errorNumber(), functionName + ": Field 'code': " +
                                     std::move(codeField).errorMessage());
  }

  auto isDeterministicSlice = slice.hasKey("isDeterministic")
                                  ? std::optional(slice.get("isDeterministic"))
                                  : std::nullopt;
  auto isDeterministic =
      arangodb::wasm::velocypack2IsDeterministic(isDeterministicSlice);
  if (!isDeterministic.ok()) {
    return ResultT<WasmFunction>::error(
        isDeterministic.errorNumber(),
        functionName + ": Field 'isDeterministic': " +
            std::move(isDeterministic).errorMessage());
  }

  return ResultT<WasmFunction>(
      WasmFunction{name.get(), {codeField.get()}, isDeterministic.get()});
}

auto arangodb::wasm::checkVelocypack2WasmFunctionIsPossible(Slice slice)
    -> Result {
  if (!slice.isObject()) {
    return Result{TRI_ERROR_BAD_PARAMETER, "Can only parse an object"};
  }

  if (!slice.hasKey("name")) {
    return Result{TRI_ERROR_BAD_PARAMETER, "Required field 'name' is missing"};
  }
  if (!slice.hasKey("code")) {
    return Result{TRI_ERROR_BAD_PARAMETER, "Required field 'code' is missing"};
  }

  std::set<std::string> validFields = {"name", "code", "isDeterministic"};
  for (const auto& field : ObjectIterator(slice)) {
    if (auto fieldname = field.key.copyString();
        !(validFields.contains(fieldname))) {
      return Result{TRI_ERROR_BAD_PARAMETER,
                    "Found unknown field '" + fieldname + "'"};
    }
  }
  return Result{};
}

auto arangodb::wasm::velocypack2Name(Slice slice) -> ResultT<std::string> {
  if (slice.isString()) {
    return slice.copyString();
  } else {
    return ResultT<std::string>::error(TRI_ERROR_BAD_PARAMETER,
                                       "Should be a string");
  }
}

auto arangodb::wasm::velocypack2Code(Slice slice) -> ResultT<Code> {
  if (slice.isArray()) {
    std::vector<uint8_t> code;
    for (auto const& p : velocypack::ArrayIterator(slice)) {
      if (p.isInteger() and p.getInt() >= 0 and p.getInt() < 256) {
        code.emplace_back(p.getInt());
      } else {
        return ResultT<Code>::error(TRI_ERROR_BAD_PARAMETER,
                                    "Array should include only bytes");
      }
    }
    return Code{code};
  } else if (slice.isString()) {
    auto string = slice.copyString();
    if (!regex_match(string, std::regex("^([A-Za-z0-9+/]{4})*([A-Za-z0-9+/"
                                        "]{3}=|[A-Za-z0-9+/]{2}==)?$"))) {
      return ResultT<Code>::error(TRI_ERROR_BAD_PARAMETER,
                                  "String should be a base64 string.");
    }
    auto decodedString =
        arangodb::basics::StringUtils::decodeBase64(slice.copyString());
    std::vector<uint8_t> vec(decodedString.begin(), decodedString.end());
    return Code{vec};
  } else {
    return ResultT<Code>::error(TRI_ERROR_BAD_PARAMETER,
                                "Sould be a byte array or base64 string");
  }
}

auto arangodb::wasm::velocypack2IsDeterministic(std::optional<Slice> slice)
    -> ResultT<bool> {
  if (!slice) {
    return false;
  }
  if (auto value = slice.value(); value.isBool()) {
    return value.getBool();
  } else {
    return ResultT<bool>::error(TRI_ERROR_BAD_PARAMETER, "Sould be a boolean");
  }
}
