#include "Validators.h"
#include <Basics/StaticStrings.h>

namespace arangodb {

  std::string_view to_view(ValidatorLevel level) {
    switch(level) {
      case ValidatorLevel::NONE: return StaticStrings::ValidatorLevelNone;
      case ValidatorLevel::NEW: return StaticStrings::ValidatorLevelNew;
      case ValidatorLevel::MODERATE: return StaticStrings::ValidatorLevelModerate;
      case ValidatorLevel::STRICT: return StaticStrings::ValidatorLevelStrict;
    }
    TRI_ASSERT(false);
    return StaticStrings::ValidatorLevelStrict; // <- avoids: reaching end of non-void function ....
  }

  //////////////////////////////////////////////////////////////////////////////

  ValidatorBase::ValidatorBase(VPackSlice params) {
    // parse message
    _message = "";
    auto msgSlice = params.get(StaticStrings::ValidatorParameterMessage);
    if(msgSlice.isString()) {
      _message = msgSlice.copyString();
    }

    // parse level
    _level = ValidatorLevel::STRICT; // default
    auto levelSlice = params.get(StaticStrings::ValidatorParameterLevel);
    if(levelSlice.isString()) {
      if (levelSlice.compareString(StaticStrings::ValidatorLevelNone) == 0){
        _level = ValidatorLevel::NONE;
      } else if(levelSlice.compareString(StaticStrings::ValidatorLevelNew) == 0) {
        _level = ValidatorLevel::NEW;
      } else if(levelSlice.compareString(StaticStrings::ValidatorLevelModerate) == 0) {
        _level = ValidatorLevel::MODERATE;
      } else if(levelSlice.compareString(StaticStrings::ValidatorLevelStrict) == 0) {
        _level = ValidatorLevel::STRICT;
      }
    }
  };

  bool ValidatorBase::validate(VPackSlice new_, VPackSlice old_, ValidatorOperation op) const {
    // this function calls the validation implementaiton depending on operation and level
    if(_level == ValidatorLevel::NONE) {
      return true;
    }

    if (op == ValidatorOperation::INSERT) {
      return validateDerived(new_);
    } else {
      // update replace case
      if( _level == ValidatorLevel::NEW) {
        // check only on insert
        return true;
      } else if ( _level == ValidatorLevel::STRICT) {
        // inserted doc must be good
        return validateDerived(new_);
      } else {
        // MODERATE case
        // doc new must good only if the old was good
        return (validateDerived(new_) || !validateDerived(old_));
      }
    }

    return false;
  }

  void ValidatorBase::toVelocyPack(VPackBuilder& b) const {
    VPackObjectBuilder gurad(&b);
    b.add(StaticStrings::ValidatorParameterMessage, VPackValue(_message));
    auto view = to_view(_level);
    b.add(StaticStrings::ValidatorParameterLevel, VPackValuePair(view.data(), view.length()));
    toVelocyPackDerived(b);
  }

  /////////////////////////////////////////////////////////////////////////////

  ValidatorBool::ValidatorBool(VPackSlice params) : ValidatorBase(params){
    _result = params.get(StaticStrings::ValidatorParameterRule).getBool();
  }
  bool ValidatorBool::validateDerived(VPackSlice slice) const {
    return _result;
  }
  void ValidatorBool::toVelocyPackDerived(VPackBuilder& b) const {
    b.add(StaticStrings::ValidatorParameterRule, VPackValue(_result));
  }
  std::string const& ValidatorBool::type() const { return StaticStrings::ValidatorTypeBool; };

  /////////////////////////////////////////////////////////////////////////////

  ValidatorAQL::ValidatorAQL(VPackSlice params) : ValidatorBase(params){
  }
  bool ValidatorAQL::validateDerived(VPackSlice slice) const {
    return true;
  }
  void ValidatorAQL::toVelocyPackDerived(VPackBuilder& b) const {
  }
  std::string const& ValidatorAQL::type() const { return StaticStrings::ValidatorTypeAQL; };

}
