// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "missing_ctor.h"

#include <string>
#include <vector>

// We don't warn on classes that use default ctors in cpp files.
class MissingInCPPOK {
 public:

 private:
  std::vector<int> one_;
  std::vector<std::string> two_;
};

int main() {
  MissingInCPPOK one;
  MissingCtorsArentOKInHeader two;
  return 0;
}
