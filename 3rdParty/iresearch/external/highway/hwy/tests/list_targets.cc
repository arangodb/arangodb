// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Simple tool to print the list of targets that were compiled in when building
// this tool.

#include <stdio.h>

#include "hwy/highway.h"

void PrintTargets(const char* msg, uint32_t targets) {
  fprintf(stderr, "%s", msg);
  for (unsigned x = targets; x != 0; x = x & (x - 1)) {
    fprintf(stderr, " %s", hwy::TargetName(x & (~x + 1)));
  }
  fprintf(stderr, "\n");
}

int main() {
  PrintTargets("Compiled HWY_TARGETS:", HWY_TARGETS);
  PrintTargets("HWY_BASELINE_TARGETS:", HWY_BASELINE_TARGETS);
  return 0;
}
