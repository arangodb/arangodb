#include <string>
#include <set>
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

WasmFunction::WasmFunction(std::string name, std::vector<uint8_t> code,
                           bool isDeterministic = false)
    : _name{std::move(name)},
      _code{std::move(code)},
      _isDeterministic{isDeterministic} {}

void WasmFunction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("name", VPackValue(_name));
  VPackBuilder arrayBuilder;
  {
    auto ab = VPackArrayBuilder(&arrayBuilder);
    for (auto const& entry : _code) {
      arrayBuilder.add(VPackValue(entry));
    }
  }
  builder.add("code", arrayBuilder.slice());
  builder.add("isDeterministic", VPackValue(_isDeterministic));
}

auto WasmFunction::fromVelocyPack(Slice slice) -> ResultT<WasmFunction> {
  std::string functionName = "WasmFunction::fromVelocyPack ";

  if (!slice.isObject()) {
    return ResultT<WasmFunction>::error(
        TRI_ERROR_BAD_PARAMETER, functionName + "Can only parse an object");
  }

  if (auto check = checkOnlyValidFieldnamesAreIncluded(
          {"name", "code", "isDeterministic"}, slice);
      check.fail()) {
    return ResultT<WasmFunction>::error(
        check.errorNumber(), functionName + std::move(check).errorMessage());
  }

  auto name = requiredStringField("name", slice);
  if (!name.ok()) {
    return ResultT<WasmFunction>::error(
        name.errorNumber(), functionName + std::move(name).errorMessage());
  }

  auto codeField = requiredCodeField("code", slice);
  if (!codeField.ok()) {
    return ResultT<WasmFunction>::error(
        codeField.errorNumber(),
        functionName + std::move(codeField).errorMessage());
  }

  auto isDeterministic = optionalBoolField("isDeterministic", false, slice);
  if (!isDeterministic.ok()) {
    return ResultT<WasmFunction>::error(
        isDeterministic.errorNumber(),
        functionName + std::move(isDeterministic).errorMessage());
  }

  return ResultT<WasmFunction>(
      WasmFunction{name.get(), {codeField.get()}, isDeterministic.get()});
}

auto WasmFunction::checkOnlyValidFieldnamesAreIncluded(
    std::set<std::string>&& validFieldnames, Slice slice) -> Result {
  for (const auto& field : ObjectIterator(slice)) {
    if (auto fieldname = field.key.copyString();
        !(validFieldnames.contains(fieldname))) {
      return Result{TRI_ERROR_BAD_PARAMETER,
                    "Found unknown field " + fieldname};
    }
  }
  return Result{};
}

auto WasmFunction::requiredStringField(std::string const&& fieldname,
                                       Slice slice) -> ResultT<std::string> {
  if (!slice.hasKey(fieldname)) {
    return ResultT<std::string>::error(
        TRI_ERROR_BAD_PARAMETER, "Required field " + fieldname + " is missing");
  }
  if (auto field = slice.get(fieldname); field.isString()) {
    return field.copyString();
  } else {
    return ResultT<std::string>::error(
        TRI_ERROR_BAD_PARAMETER, "Field " + fieldname + " should be a string");
  }
}

auto WasmFunction::requiredCodeField(std::string const&& fieldname, Slice slice)
    -> ResultT<std::vector<uint8_t>> {
  if (!slice.hasKey(fieldname)) {
    return ResultT<std::vector<uint8_t>>::error(
        TRI_ERROR_BAD_PARAMETER, "Required field " + fieldname + " is missing");
  }
  if (auto field = slice.get(fieldname); field.isArray()) {
    // if field is base64: arangodb::baseics::StringUtils::encodeBase64(field)
    std::vector<uint8_t> code;
    for (auto const& p : velocypack::ArrayIterator(field)) {
      // TODO check that p is of byte type
      code.emplace_back(p.getInt());
    }
    return code;
    // } else if (field.isString()) {
  } else {
    return ResultT<std::vector<uint8_t>>::error(
        TRI_ERROR_BAD_PARAMETER,
        "Field " + fieldname + " should be a byte array");
  }
}

auto WasmFunction::optionalBoolField(std::string const&& fieldname,
                                     bool defaultValue, Slice slice)
    -> ResultT<bool> {
  if (!slice.hasKey(fieldname)) {
    return defaultValue;
  }
  if (auto field = slice.get(fieldname); field.isBool()) {
    return field.getBool();
  } else {
    return ResultT<bool>::error(TRI_ERROR_BAD_PARAMETER,
                                "Field " + fieldname + " should be a boolean");
  }
}
