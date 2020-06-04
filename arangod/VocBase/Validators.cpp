////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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


#include "Validators.h"
#include <Basics/Exceptions.h>
#include <Basics/StaticStrings.h>
#include <Logger/LogMacros.h>

#include <tao/json/contrib/schema.hpp>
#include <tao/json/jaxn/to_string.hpp>
#include <validation/validation.hpp>


#include <iostream>
#include <tao/json/to_string.hpp>

namespace arangodb {

std::string const&  to_string(ValidationLevel level) {
  switch (level) {
    case ValidationLevel::None:
      return StaticStrings::ValidationLevelNone;
    case ValidationLevel::New:
      return StaticStrings::ValidationLevelNew;
    case ValidationLevel::Moderate:
      return StaticStrings::ValidationLevelModerate;
    case ValidationLevel::Strict:
      return StaticStrings::ValidationLevelStrict;
  }
  TRI_ASSERT(false);
  return StaticStrings::ValidationLevelStrict;  // <- avoids: reaching end of non-void function ....
}

//////////////////////////////////////////////////////////////////////////////

ValidatorBase::ValidatorBase(VPackSlice params)
    : _level(ValidationLevel::Strict), _special(validation::SpecialProperties::None) {
  // parse message
  auto msgSlice = params.get(StaticStrings::ValidationParameterMessage);
  if (msgSlice.isString()) {
    this->_message = msgSlice.copyString();
  }

  // parse level
  auto levelSlice = params.get(StaticStrings::ValidationParameterLevel);
  if (!levelSlice.isNone() && levelSlice.isString()) {
    if (levelSlice.compareString(StaticStrings::ValidationLevelNone) == 0) {
      this->_level = ValidationLevel::None;
    } else if (levelSlice.compareString(StaticStrings::ValidationLevelNew) == 0) {
      this->_level = ValidationLevel::New;
    } else if (levelSlice.compareString(StaticStrings::ValidationLevelModerate) == 0) {
      this->_level = ValidationLevel::Moderate;
    } else if (levelSlice.compareString(StaticStrings::ValidationLevelStrict) == 0) {
      this->_level = ValidationLevel::Strict;
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_VALIDATION_BAD_PARAMETER,
                                     "Valid validation levels are: " + StaticStrings::ValidationLevelNone +
                                         ", " + StaticStrings::ValidationLevelNew +
                                         ", " + StaticStrings::ValidationLevelModerate +
                                         ", " + StaticStrings::ValidationLevelStrict);
    }
  }
}

Result ValidatorBase::validate(VPackSlice newDoc, VPackSlice oldDoc, bool isInsert, VPackOptions const* options) const {
  // This function performs the validation depending on operation (Insert /
  // Update / Replace) and requested validation level (None / Insert / New /
  // Strict / Moderate).

  if (this->_level == ValidationLevel::None) {
    return {};
  }

  if (isInsert) {
    return this->validateOne(newDoc, options);
  }

  /* update replace case */
  if (this->_level == ValidationLevel::New) {
    // Level NEW is for insert only.
    return {};
  }

  if (this->_level == ValidationLevel::Strict) {
    // Changed document must be good!
    return validateOne(newDoc, options);
  }

  TRI_ASSERT(this->_level == ValidationLevel::Moderate);
  // Changed document must be good IIF the unmodified
  // document passed validation.

  auto resNew = this->validateOne(newDoc, options);
  if (resNew.ok()) {
    return {};
  }

  if (this->validateOne(oldDoc, options).fail()) {
    return {};
  }

  return resNew;
}

void ValidatorBase::toVelocyPack(VPackBuilder& b) const {
  VPackObjectBuilder guard(&b);
  b.add(StaticStrings::ValidationParameterMessage, VPackValue(_message));
  b.add(StaticStrings::ValidationParameterLevel, VPackValue(to_string(this->_level)));
  this->toVelocyPackDerived(b);
}

/////////////////////////////////////////////////////////////////////////////

ValidatorBool::ValidatorBool(VPackSlice params) : ValidatorBase(params) {
  _result = params.get(StaticStrings::ValidationParameterRule).getBool();
}
Result ValidatorBool::validateOne(VPackSlice slice, VPackOptions const* options) const {
  if (_result) {
    return {};
  }
  return {TRI_ERROR_VALIDATION_FAILED, _message};
}

void ValidatorBool::toVelocyPackDerived(VPackBuilder& b) const {
  b.add(StaticStrings::ValidationParameterRule, VPackValue(_result));
}

std::string const& ValidatorBool::type() const {
  static std::string const rv = std::string("bool");
  return rv;
}

/////////////////////////////////////////////////////////////////////////////

ValidatorJsonSchema::ValidatorJsonSchema(VPackSlice params) : ValidatorBase(params) {
  auto rule = params.get(StaticStrings::ValidationParameterRule);
  if (!rule.isObject()) {
    std::string msg = "No valid schema in rule attribute given (no object): ";
    msg += params.toJson();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_VALIDATION_BAD_PARAMETER, msg);
  }
  auto taoRuleValue = validation::slice_to_value(rule);
  try {
    _schema = std::make_shared<tao::json::schema>(taoRuleValue);
    _builder.add(rule);
  } catch (std::exception const& ex) {
    auto valueString =  tao::json::to_string(taoRuleValue, 4);
    auto msg = std::string("invalid object") + valueString + "exception: " + ex.what();
    LOG_TOPIC("baabe", ERR, Logger::VALIDATION) << msg;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_VALIDATION_BAD_PARAMETER, msg);
  }
}

Result ValidatorJsonSchema::validateOne(VPackSlice slice, VPackOptions const* options) const {
  auto res = validation::validate(*_schema, _special, slice, options);
  if (res) {
    return {};
  }
  return {TRI_ERROR_VALIDATION_FAILED, _message};
}
void ValidatorJsonSchema::toVelocyPackDerived(VPackBuilder& b) const {
  TRI_ASSERT(!_builder.slice().isNone());
  b.add(StaticStrings::ValidationParameterRule, _builder.slice());
}
std::string const& ValidatorJsonSchema::type() const {
  static std::string const rv = std::string("json");
  return rv;
}

}  // namespace arangodb
