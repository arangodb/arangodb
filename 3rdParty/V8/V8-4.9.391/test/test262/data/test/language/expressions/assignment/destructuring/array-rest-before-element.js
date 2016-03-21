// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    An AssignmentElement may not follow an AssignmentRestElement in an
    AssignmentElementList.
es6id: 12.14.5
negative: SyntaxError
---*/

var x, y;

[...x, y] = [];
