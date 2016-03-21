// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    ArrayAssignmentPattern may not include elisions following an
    AssignmentRestElement in a AssignmentElementList.
es6id: 12.14.5
negative: SyntaxError
---*/

[...x,] = [];
