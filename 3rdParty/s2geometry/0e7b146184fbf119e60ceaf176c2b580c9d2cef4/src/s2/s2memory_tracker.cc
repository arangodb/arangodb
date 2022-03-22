// Copyright 2020 Google Inc. All Rights Reserved.
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

#include "s2/s2memory_tracker.h"

void S2MemoryTracker::SetError(S2Error error) {
  error_ = std::move(error);
}

// Not inline in order to avoid code bloat.
void S2MemoryTracker::SetLimitExceededError() {
  error_.Init(S2Error::RESOURCE_EXHAUSTED,
              "Memory limit exceeded (tracked usage %d bytes, limit %d bytes)",
              usage_bytes_, limit_bytes_);
}

bool S2MemoryTracker::Client::TallyTemp(int64 delta_bytes) {
  Tally(delta_bytes);
  return Tally(-delta_bytes);
}

const S2Error& S2MemoryTracker::Client::error() const {
  static S2Error error_ok;  // NOLINT
  return tracker_ ? tracker_->error() : error_ok;
}
