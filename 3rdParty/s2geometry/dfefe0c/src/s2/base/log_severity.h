// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef S2_BASE_LOG_SEVERITY_H_
#define S2_BASE_LOG_SEVERITY_H_

#ifdef S2_USE_GLOG

#include <glog/log_severity.h>

#else  // !defined(S2_USE_GLOG)

#include "s2/third_party/absl/base/log_severity.h"

// Stay compatible with glog.
namespace google {

#ifdef NDEBUG
constexpr bool DEBUG_MODE = false;
#else
constexpr bool DEBUG_MODE = true;
#endif

}  // namespace google

#endif  // !defined(S2_USE_GLOG)

#endif  // S2_BASE_LOG_SEVERITY_H_
