////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

// This is a temporary solution to allow us to move logic to declare/validate
// options out of the features without changing the API of the features
// themselves. The option values still have to live in the features because
// ProgramOptions::addOption stores a reference to the value. That's why this
// class is templated with the Options type and takes the options struct as
// argument.
// Long term we want to remove this and make the provide a pure virtual class.
// To get there, we have to remove the option related API from the features and
// instead add an additional phase in the startup code that instantiates the
// different concrete options providers. Then we can process program options
// _before_ creating the features and can pass the already parsed options to
// the features when they are created.
template<typename OptionsT>
struct OptionsProvider {
  using OptionsType = OptionsT;

  virtual ~OptionsProvider() = default;

  // Declare options, binding them to the provided options struct
  virtual void declareOptions(std::shared_ptr<options::ProgramOptions> opts,
                              OptionsT& options) = 0;

  // Optional callback to validate options
  virtual void validateOptions(std::shared_ptr<options::ProgramOptions> opts,
                               OptionsT& options) {}
};

}  // namespace arangodb
