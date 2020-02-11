#include "Validators.h"
#include <Basics/StaticStrings.h>

namespace arangodb {

std::string_view to_view(ValidatorLevel level) {
  switch (level) {
    case ValidatorLevel::NONE:
      return StaticStrings::ValidatorLevelNone;
    case ValidatorLevel::NEW:
      return StaticStrings::ValidatorLevelNew;
    case ValidatorLevel::MODERATE:
      return StaticStrings::ValidatorLevelModerate;
    case ValidatorLevel::STRICT:
      return StaticStrings::ValidatorLevelStrict;
  }
  TRI_ASSERT(false);
  return StaticStrings::ValidatorLevelStrict;  // <- avoids: reaching end of non-void function ....
}

//////////////////////////////////////////////////////////////////////////////

ValidatorBase::ValidatorBase(VPackSlice params) {
  // parse message
  _message = "";
  auto msgSlice = params.get(StaticStrings::ValidatorParameterMessage);
  if (msgSlice.isString()) {
    this->_message = msgSlice.copyString();
  }

  // parse level
  _level = ValidatorLevel::STRICT;  // default as defined in design document
  auto levelSlice = params.get(StaticStrings::ValidatorParameterLevel);
  if (levelSlice.isString()) {
    if (levelSlice.compareString(StaticStrings::ValidatorLevelNone) == 0) {
      this->_level = ValidatorLevel::NONE;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelNew) == 0) {
      this->_level = ValidatorLevel::NEW;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelModerate) == 0) {
      this->_level = ValidatorLevel::MODERATE;
    } else if (levelSlice.compareString(StaticStrings::ValidatorLevelStrict) == 0) {
      this->_level = ValidatorLevel::STRICT;
    }
  }
};

bool ValidatorBase::validate(VPackSlice new_, VPackSlice old_, ValidatorOperation op) const {
  // This function performs the validation depending on operation (insert /
  // update / replace) and requested validation level (NONE / INSERT / NEW /
  // STRICT / MODERATE).
  if (this->_level == ValidatorLevel::NONE) {
    return true;
  }

  if (op == ValidatorOperation::INSERT) {
    return this->validateDerived(new_);
  } else /* update replace case */ {
    if (this->_level == ValidatorLevel::NEW) {
      // Level NEW is for insert only.
      return true;
    } else if (this->_level == ValidatorLevel::STRICT) {
      // Changed document must be good!
      return validateDerived(new_);
    } else /* ValidatorLevel::MODERATE */ {
      // Changed document must be good IIF the unmodified
      // document passed validation.
      return (this->validateDerived(new_) || !this->validateDerived(old_));
    }
  }

  return false;
}

void ValidatorBase::toVelocyPack(VPackBuilder& b) const {
  VPackObjectBuilder gurad(&b);
  auto view = to_view(this->_level);
  b.add(StaticStrings::ValidatorParameterLevel,
        VPackValuePair(view.data(), view.length()));
  b.add("type", VPackValuePair(this->type().data(), this->type().length()));
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
};

}  // namespace arangodb
