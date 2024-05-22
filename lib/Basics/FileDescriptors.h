////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Basics/operating-system.h"

#include <limits>
#include <string>

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef TRI_HAVE_GETRLIMIT
namespace arangodb {

struct FileDescriptors {
  using ValueType = rlim_t;

  static constexpr ValueType requiredMinimum = 1024;
  static constexpr ValueType maximumValue =
      std::numeric_limits<ValueType>::max();

  ValueType hard;
  ValueType soft;

  static Result load(FileDescriptors& descriptors);

  static Result store(FileDescriptors const& descriptors);

  static Result adjustTo(ValueType value);

  static ValueType recommendedMinimum();

  static bool isUnlimited(ValueType value);

  static std::string stringify(ValueType value);
};

}  // namespace arangodb
#endif
