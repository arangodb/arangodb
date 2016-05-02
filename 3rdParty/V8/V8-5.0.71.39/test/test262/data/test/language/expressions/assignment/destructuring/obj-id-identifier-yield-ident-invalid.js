// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    `yield` is not a valid IdentifierReference in an AssignmentProperty within
    strict mode code.
es6id: 12.14.5
flags: [onlyStrict]
negative: SyntaxError
---*/

var { yield } = {};
