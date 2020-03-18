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
      return StaticStrings::ValidatorLevelNone;
    case ValidationLevel::New:
      return StaticStrings::ValidatorLevelNew;
    case ValidationLevel::Moderate:
      return StaticStrings::ValidatorLevelModerate;
    case ValidationLevel::Strict:
      return StaticStrings::ValidatorLevelStrict;
  }
  TRI_ASSERT(false);
  return StaticStrings::ValidatorLevelStrict;  // <- avoids: reaching end of non-void function ....
}

std::string const&  to_string(validation::SpecialProperties special) {
  switch (special) {
    case validation::SpecialProperties::All:
      return StaticStrings::ValidatorPropertyAll;
    case validation::SpecialProperties::None:
      return StaticStrings::ValidatorPropertyNone;
    case validation::SpecialProperties::Key:
    case validation::SpecialProperties::Id:
    case validation::SpecialProperties::Rev:
    case validation::SpecialProperties::From:
    case validation::SpecialProperties::To:
      return StaticStrings::ValidatorPropertyNone;
  }
  TRI_ASSERT(false);
  return StaticStrings::ValidatorPropertyNone;  // <- avoids: reaching end of non-void function ....
}


//////////////////////////////////////////////////////////////////////////////

ValidatorBase::ValidatorBase(VPackSlice params)
    : _level(ValidationLevel::Strict), _special(validation::SpecialProperties::None) {
  // parse message
  auto msgSlice = params.get(StaticStrings::ValidatorParameterMessage);
  if (msgSlice.isString()) {
    this->_message = msgSlice.copyString();
  }

  // parse level
  auto levelSlice = params.get(StaticStrings::ValidatorParameterLevel);
  if (!levelSlice.isNone() && levelSlice.isString()) {
    if (levelSlice.compareString(StaticStrings::ValidatorLevelNone) == 0) {
      this->_level = ValidationLevel::None;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelNew) == 0) {
      this->_level = ValidationLevel::New;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelModerate) == 0) {
      this->_level = ValidationLevel::Moderate;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelStrict) == 0) {
      this->_level = ValidationLevel::Strict;
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_VALIDATION_BAD_PARAMETER,
                                     "Valid validation levels are: " + StaticStrings::ValidatorLevelNone +
                                         ", " + StaticStrings::ValidatorLevelNew +
                                         ", " + StaticStrings::ValidatorLevelModerate +
                                         ", " + StaticStrings::ValidatorLevelStrict);
    }
  }

  // special attributes
  // eventually we want something like:
  // "key|from|to" and not only "all" or "none"
  auto specialSlice = params.get(StaticStrings::ValidatorParameterSpecialProperties);
  if (!specialSlice.isNone() && specialSlice.isString()) {
    if (specialSlice.compareString(StaticStrings::ValidatorPropertyNone) == 0) {
      this->_special = validation::SpecialProperties::None;
    } else if (specialSlice.compareString(StaticStrings::ValidatorPropertyAll) == 0) {
      this->_special = validation::SpecialProperties::All;
    } else {
      using namespace std::literals::string_literals;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_VALIDATION_BAD_PARAMETER,
                                     "Valid built-in attributes are: "s + StaticStrings::ValidatorPropertyNone +
                                         ", " + StaticStrings::ValidatorPropertyAll +
                                         "");
    }
  }
}

bool ValidatorBase::validate(VPackSlice newDoc, VPackSlice oldDoc, bool isInsert, VPackOptions const* options) const {
  // This function performs the validation depending on operation (Insert /
  // Update / Replace) and requested validation level (None / Insert / New /
  // Strict / Moderate).

  if (this->_level == ValidationLevel::None) {
    return true;
  }

  if (isInsert) {
    return this->validateDerived(newDoc, options);
  }

  /* update replace case */
  if (this->_level == ValidationLevel::New) {
    // Level NEW is for insert only.
    return true;
  }

  if (this->_level == ValidationLevel::Strict) {
    // Changed document must be good!
    return validateDerived(newDoc, options);
  }

  TRI_ASSERT(this->_level == ValidationLevel::Moderate);
  // Changed document must be good IIF the unmodified
  // document passed validation.
  return (this->validateDerived(newDoc, options) || !this->validateDerived(oldDoc, options));
}

void ValidatorBase::toVelocyPack(VPackBuilder& b) const {
  VPackObjectBuilder guard(&b);
  b.add(StaticStrings::ValidatorParameterMessage, VPackValue(_message));
  b.add(StaticStrings::ValidatorParameterLevel, VPackValue(to_string(this->_level)));
  b.add(StaticStrings::ValidatorParameterType, VPackValue(this->type()));
  b.add(StaticStrings::ValidatorParameterSpecialProperties, VPackValue(to_string(this->_special)));
  this->toVelocyPackDerived(b);
}

/////////////////////////////////////////////////////////////////////////////

ValidatorBool::ValidatorBool(VPackSlice params) : ValidatorBase(params) {
  _result = params.get(StaticStrings::ValidatorParameterRule).getBool();
}
bool ValidatorBool::validateDerived(VPackSlice slice, VPackOptions const* options) const { return _result; }
void ValidatorBool::toVelocyPackDerived(VPackBuilder& b) const {
  b.add(StaticStrings::ValidatorParameterRule, VPackValue(_result));
}
std::string const& ValidatorBool::type() const {
  return StaticStrings::ValidatorTypeBool;
}

/////////////////////////////////////////////////////////////////////////////

ValidatorJsonSchema::ValidatorJsonSchema(VPackSlice params) : ValidatorBase(params) {
  auto rule = params.get(StaticStrings::ValidatorParameterRule);
  if (!rule.isObject()) {
    std::string msg = "No valid schema in rule attribute given (no object): ";
    msg += params.toJson();
    LOG_TOPIC("ababf", ERR, Logger::VALIDATION) << msg;
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
bool ValidatorJsonSchema::validateDerived(VPackSlice slice, VPackOptions const* options) const {
  return validation::validate(*_schema, _special, slice, options);
}
void ValidatorJsonSchema::toVelocyPackDerived(VPackBuilder& b) const {
  TRI_ASSERT(!_builder.slice().isNone());
  b.add(StaticStrings::ValidatorParameterRule, _builder.slice());
}
std::string const& ValidatorJsonSchema::type() const {
  return StaticStrings::ValidatorTypeJsonSchema;
}

}  // namespace arangodb
