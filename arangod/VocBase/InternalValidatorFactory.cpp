////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "InternalValidatorFactory.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/InternalValidatorFactoryEE.h"
#endif

#include "Basics/StaticStrings.h"
#include "VocBase/Validators.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"

using namespace arangodb;

ResultT<std::unique_ptr<arangodb::ValidatorBase>> InternalValidatorFactory::ValidatorFromSlice(VPackSlice definition) {
  VPackSlice typeSlice = definition.get(StaticStrings::ValidationParameterType);
  if (!typeSlice.isString()) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  "Failed to create Validator, '" +
                      StaticStrings::ValidationParameterType + "' is missing"};
  }
  VPackStringRef type(typeSlice);

#ifdef USE_ENTERPRISE
  auto res = InternalValidatorFactoryEE::ValidatorFromSlice(type, definition);
#endif
  if (res.ok() || res.result().isNot(TRI_ERROR_TYPE_ERROR)) {
    return res;
  }
  return Result{TRI_ERROR_TYPE_ERROR, "ValidatorType not known."};
}
