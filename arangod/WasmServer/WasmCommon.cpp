#include <string>
#include <set>
#include "WasmCommon.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "velocypack/vpack.h"
#include "Basics/ResultT.h"
#include "Basics/Result.h"

using namespace arangodb;
using namespace arangodb::wasm;
using namespace arangodb::velocypack;

WasmFunction::WasmFunction(std::string name, std::string code,
                           bool isDeterministic = false)
    : _name{std::move(name)},
      _code{std::move(code)},
      _isDeterministic{isDeterministic} {}

void WasmFunction::toVelocyPack(VPackBuilder& builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("name", VPackValue(_name));
  builder.add("code", VPackValue(_code));
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

  auto nameField = requiredStringField("name", slice);
  if (!nameField.ok()) {
    return ResultT<WasmFunction>::error(
        nameField.errorNumber(),
        functionName + std::move(nameField).errorMessage());
  }
  auto name = nameField.get().copyString();

  auto codeField = requiredStringField("code", slice);
  if (!codeField.ok()) {
    return ResultT<WasmFunction>::error(
        codeField.errorNumber(),
        functionName + std::move(codeField).errorMessage());
  }
  auto code = codeField.get().copyString();

  auto isDeterministicField =
      optionalBoolField("isDeterministic", false, slice);
  if (!isDeterministicField.ok()) {
    return ResultT<WasmFunction>::error(
        isDeterministicField.errorNumber(),
        functionName + std::move(isDeterministicField).errorMessage());
  }
  auto isDeterministic = isDeterministicField.get();

  return ResultT<WasmFunction>(WasmFunction{name, code, isDeterministic});
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
                                       Slice slice) -> ResultT<Slice> {
  if (!slice.hasKey(fieldname)) {
    return ResultT<Slice>::error(TRI_ERROR_BAD_PARAMETER,
                                 "Required field " + fieldname + " is missing");
  }
  if (auto field = slice.get(fieldname); field.isString()) {
    return field;
  } else {
    return ResultT<Slice>::error(TRI_ERROR_BAD_PARAMETER,
                                 "Field " + fieldname + " should be a string");
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
