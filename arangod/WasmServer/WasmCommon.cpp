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

using namespace arangodb;
using namespace arangodb::wasm;
using namespace arangodb::velocypack;

void codeToVelocypack(Code const& code, VPackBuilder& builder) {
  auto ab = VPackArrayBuilder(&builder);
  for (auto const& entry : code.bytes) {
    builder.add(VPackValue(entry));
  }
}

void arangodb::wasm::moduleToVelocypack(Module const& module,
                                        VPackBuilder& builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("name", VPackValue(module.name));
  builder.add(VPackValue("code"));
  codeToVelocypack(module.code, builder);
  builder.add("isDeterministic", VPackValue(module.isDeterministic));
}

auto checkVelocypackToModuleIsPossible(Slice slice) -> Result {
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

auto velocypackToName(Slice slice) -> ResultT<std::string> {
  if (slice.isString()) {
    return slice.copyString();
  } else {
    return ResultT<std::string>::error(TRI_ERROR_BAD_PARAMETER,
                                       "Should be a string");
  }
}

auto velocypackToCode(Slice slice) -> ResultT<Code> {
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
    if (!regex_match(string,
                     std::regex(arangodb::basics::StringUtils::base64Regex))) {
      return ResultT<Code>::error(TRI_ERROR_BAD_PARAMETER,
                                  "String should be a base64 string.");
    }
    auto decodedString =
        arangodb::basics::StringUtils::decodeBase64(slice.copyString());
    std::vector<uint8_t> vec(decodedString.begin(), decodedString.end());
    return Code{vec};
  } else {
    return ResultT<Code>::error(TRI_ERROR_BAD_PARAMETER,
                                "Should be a byte array or base64 string");
  }
}

auto velocypackToIsDeterministic(std::optional<Slice> slice) -> ResultT<bool> {
  if (!slice) {
    return false;
  }
  if (auto value = slice.value(); value.isBool()) {
    return value.getBool();
  } else {
    return ResultT<bool>::error(TRI_ERROR_BAD_PARAMETER, "Should be a boolean");
  }
}

auto arangodb::wasm::velocypackToModule(Slice slice) -> ResultT<Module> {
  std::string functionName = "wasm::velocypackToModule";

  if (auto check = checkVelocypackToModuleIsPossible(slice); check.fail()) {
    return ResultT<Module>::error(
        check.errorNumber(), functionName + std::move(check).errorMessage());
  }

  auto name = velocypackToName(slice.get("name"));
  if (!name.ok()) {
    return ResultT<Module>::error(
        name.errorNumber(),
        functionName + ": Field 'name': " + std::move(name).errorMessage());
  }

  auto codeField = velocypackToCode(slice.get("code"));
  if (!codeField.ok()) {
    return ResultT<Module>::error(codeField.errorNumber(),
                                  functionName + ": Field 'code': " +
                                      std::move(codeField).errorMessage());
  }

  auto isDeterministicSlice = slice.hasKey("isDeterministic")
                                  ? std::optional(slice.get("isDeterministic"))
                                  : std::nullopt;
  auto isDeterministic = velocypackToIsDeterministic(isDeterministicSlice);
  if (!isDeterministic.ok()) {
    return ResultT<Module>::error(
        isDeterministic.errorNumber(),
        functionName + ": Field 'isDeterministic': " +
            std::move(isDeterministic).errorMessage());
  }

  return ResultT<Module>(
      Module{name.get(), {codeField.get()}, isDeterministic.get()});
}

auto uint64FromSlice(Slice slice) -> std::optional<uint64_t> {
  if (slice.isSmallInt()) {
    auto r = slice.getSmallInt();
    if (r >= 0) {
      return r;
    }
  }
  if (slice.isUInt()) {
    return slice.getUInt();
  }
  return std::nullopt;
}

auto arangodb::wasm::velocypackToFunctionParameters(Slice slice)
    -> ResultT<FunctionParameters> {
  if (!slice.isObject()) {
    return ResultT<FunctionParameters>::error(TRI_ERROR_BAD_PARAMETER,
                                              "Can only parse an object");
  }
  if (!slice.hasKey("a")) {
    return ResultT<FunctionParameters>::error(TRI_ERROR_BAD_PARAMETER,
                                              "Required field 'a' is missing");
  }
  if (!slice.hasKey("b")) {
    return ResultT<FunctionParameters>::error(TRI_ERROR_BAD_PARAMETER,
                                              "Required field 'b' is missing");
  }
  auto a = uint64FromSlice(slice.get("a"));
  if (!a.has_value()) {
    return ResultT<FunctionParameters>::error(
        TRI_ERROR_BAD_PARAMETER, "Field a: Should be an unsigned integer");
  }
  auto b = uint64FromSlice(slice.get("b"));
  if (!b.has_value()) {
    return ResultT<FunctionParameters>::error(
        TRI_ERROR_BAD_PARAMETER, "Field b: Should be an unsigned integer");
  }
  return FunctionParameters{a.value(), b.value()};
}
