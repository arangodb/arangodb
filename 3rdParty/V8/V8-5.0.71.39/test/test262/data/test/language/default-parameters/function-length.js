// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.1.6
description: >
  Default parameters' effect on function length
info: >
  Function length is counted by the non initialized parameters in the left.

  FormalsList : FormalParameter

    1. If HasInitializer of FormalParameter is true return 0
    2. Return 1.

  FormalsList : FormalsList , FormalParameter

    1. Let count be the ExpectedArgumentCount of FormalsList.
    2. If HasInitializer of FormalsList is true or HasInitializer of
    FormalParameter is true, return count.
    3. Return count+1.
---*/

assert.sameValue((function (x = 42) {}).length, 0);
assert.sameValue((function (x = 42, y) {}).length, 0);
assert.sameValue((function (x, y = 42) {}).length, 1);
assert.sameValue((function (x, y = 42, z) {}).length, 1);
