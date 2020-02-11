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

#ifndef ARANGOD_VOC_BASE_VALIDATORS_H
#define ARANGOD_VOC_BASE_VALIDATORS_H 1

#include <Basics/debugging.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>
#include <string>

namespace arangodb {

enum class ValidatorLevel {
  NONE = 0,
  NEW = 1,
  MODERATE = 2,
  STRICT = 3,
};

enum class ValidatorOperation {
  INSERT = 0,
  UPDATE = 1,
  REPLACE = 2,
};

struct ValidatorBase {
  ValidatorBase(VPackSlice params);
  virtual ~ValidatorBase() = default;

  bool validate(VPackSlice new_, VPackSlice old_, ValidatorOperation op) const;
  void toVelocyPack(VPackBuilder&) const;
  virtual std::string const& type() const = 0;
  std::string const& message() const { return this->_message; };

 protected:
  virtual bool validateDerived(VPackSlice slice) const = 0;
  virtual void toVelocyPackDerived(VPackBuilder&) const = 0;

  std::string _message;
  ValidatorLevel _level;
};

struct ValidatorAQL : public ValidatorBase {
  ValidatorAQL(VPackSlice params);
  bool validateDerived(VPackSlice slice) const override;
  void toVelocyPackDerived(VPackBuilder& b) const override;
  std::string const& type() const override;
};

struct ValidatorBool : public ValidatorBase {
  ValidatorBool(VPackSlice params);
  bool validateDerived(VPackSlice slice) const override;
  void toVelocyPackDerived(VPackBuilder& b) const override;
  std::string const& type() const override;

 private:
  bool _result;
};

}  // namespace arangodb
#endif
