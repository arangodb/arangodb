// Copyright 2013 Google Inc. All Rights Reserved.
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

// Author: ericv@google.com (Eric Veach)

#include "s2/s2error.h"

#include "s2/base/stringprintf.h"

void S2Error::Init(Code code, const char* format, ...) {
  code_ = code;
  text_.clear();
  va_list ap;
  va_start(ap, format);
  StringAppendV(&text_, format, ap);
  va_end(ap);
}
