// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Files: test/mjsunit/code-coverage-utils.js

%DebugToggleBlockCoverage(true);

try {
  throw new Error();
} catch (e) {
  e.stack;
}
