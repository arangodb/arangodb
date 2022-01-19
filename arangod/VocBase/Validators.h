////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Basics/debugging.h>
#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <string>

#include <tao/json/forward.hpp>
#include <validation/types.hpp>

namespace tao::json {
template<template<typename...> class Traits>
class basic_schema;
}

namespace arangodb {

enum class ValidationLevel {
  None = 0,
  New = 1,
  Moderate = 2,
  Strict = 3,
};

struct ValidatorBase {
 protected:
  explicit ValidatorBase();

 public:
  explicit ValidatorBase(VPackSlice params);
  virtual ~ValidatorBase() = default;

  // Validation function as it should be used in the logical collection or
  // storage engine.
  virtual Result validate(VPackSlice newDoc, VPackSlice oldDoc, bool isInsert,
                          VPackOptions const*) const;

  // Validate a single document in the specialized class ignoring the the level.
  // This version is used in the implementations of AQL Functions.
  virtual Result validateOne(VPackSlice slice, VPackOptions const*) const = 0;

  void toVelocyPack(VPackBuilder&) const;
  virtual char const* type() const = 0;
  std::string const& message() const { return this->_message; }
  std::string const& specialProperties() const;
  void setLevel(ValidationLevel level) noexcept { _level = level; }
  ValidationLevel level() { return _level; }

 protected:
  virtual void toVelocyPackDerived(VPackBuilder&) const = 0;

  std::string _message;
  ValidationLevel _level;
  validation::SpecialProperties _special;
};

struct ValidatorJsonSchema : public ValidatorBase {
  explicit ValidatorJsonSchema(VPackSlice params);
  Result validateOne(VPackSlice slice, VPackOptions const*) const override;
  void toVelocyPackDerived(VPackBuilder& b) const override;
  char const* type() const override;

 private:
  std::shared_ptr<tao::json::basic_schema<tao::json::traits>> _schema;
  VPackBuilder _builder;
};

struct ValidatorBool : public ValidatorBase {
  explicit ValidatorBool(VPackSlice params);
  Result validateOne(VPackSlice slice, VPackOptions const*) const override;
  void toVelocyPackDerived(VPackBuilder& b) const override;
  char const* type() const override;

 private:
  bool _result;
};

}  // namespace arangodb
