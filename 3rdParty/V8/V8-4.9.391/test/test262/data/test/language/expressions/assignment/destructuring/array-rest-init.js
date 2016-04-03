// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The AssignmentRestElement does not support an initializer.
es6id: 12.14.5
negative: SyntaxError
---*/

var x;

[...x = 1] = [];
