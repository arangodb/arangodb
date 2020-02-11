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
#include <Basics/StaticStrings.h>

namespace arangodb {

std::string_view to_view(ValidationLevel level) {
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

//////////////////////////////////////////////////////////////////////////////

ValidatorBase::ValidatorBase(VPackSlice params) : _level(ValidationLevel::Strict) {
  // parse message
  auto msgSlice = params.get(StaticStrings::ValidatorParameterMessage);
  if (msgSlice.isString()) {
    this->_message = msgSlice.copyString();
  }

  // parse level
  auto levelSlice = params.get(StaticStrings::ValidatorParameterLevel);
  if (levelSlice.isString()) {
    if (levelSlice.compareString(StaticStrings::ValidatorLevelNone) == 0) {
      this->_level = ValidationLevel::None;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelNew) == 0) {
      this->_level = ValidationLevel::New;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelModerate) == 0) {
      this->_level = ValidationLevel::Moderate;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelStrict) == 0) {
      this->_level = ValidationLevel::Strict;
    }
  }
}

bool ValidatorBase::validate(VPackSlice new_, VPackSlice old_, bool isInsert) const {
  // This function performs the validation depending on operation (Insert /
  // Update / Replace) and requested validation level (None / Insert / New /
  // Strict / Moderate).
  if (this->_level == ValidationLevel::None) {
    return true;
  }

  if (isInsert) {
    return this->validateDerived(new_);
  }

  /* update replace case */
  if (this->_level == ValidationLevel::New) {
    // Level NEW is for insert only.
    return true;
  }

  if (this->_level == ValidationLevel::Strict) {
    // Changed document must be good!
    return validateDerived(new_);
  }

  TRI_ASSERT(this->_level == ValidationLevel::Moderate);
  // Changed document must be good IIF the unmodified
  // document passed validation.
  return (this->validateDerived(new_) || !this->validateDerived(old_));
}

void ValidatorBase::toVelocyPack(VPackBuilder& b) const {
  VPackObjectBuilder guard(&b);
  auto view = to_view(this->_level);
  b.add(StaticStrings::ValidatorParameterLevel,
        VPackValuePair(view.data(), view.length()));
  b.add(StaticStrings::ValidatorParameterType,
        VPackValuePair(this->type().data(), this->type().length()));
  b.add(StaticStrings::ValidatorParameterMessage, VPackValue(_message));
  this->toVelocyPackDerived(b);
}

/////////////////////////////////////////////////////////////////////////////

ValidatorBool::ValidatorBool(VPackSlice params) : ValidatorBase(params) {
  _result = params.get(StaticStrings::ValidatorParameterRule).getBool();
}
bool ValidatorBool::validateDerived(VPackSlice slice) const { return _result; }
void ValidatorBool::toVelocyPackDerived(VPackBuilder& b) const {
  b.add(StaticStrings::ValidatorParameterRule, VPackValue(_result));
}
std::string const& ValidatorBool::type() const {
  return StaticStrings::ValidatorTypeBool;
};

/////////////////////////////////////////////////////////////////////////////

ValidatorAQL::ValidatorAQL(VPackSlice params) : ValidatorBase(params) {}
bool ValidatorAQL::validateDerived(VPackSlice slice) const { return true; }
void ValidatorAQL::toVelocyPackDerived(VPackBuilder& b) const {}
std::string const& ValidatorAQL::type() const {
  return StaticStrings::ValidatorTypeAQL;
}

}  // namespace arangodb
