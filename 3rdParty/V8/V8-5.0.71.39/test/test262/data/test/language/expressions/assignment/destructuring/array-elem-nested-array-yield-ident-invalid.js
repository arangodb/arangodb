// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the DestructuringAssignmentTarget of a
    nested destructuring assignment outside of strict mode, it behaves as an
    IdentifierReference.
es6id: 12.14.5.3
flags: [onlyStrict]
negative: SyntaxError
---*/

[[x[yield]]] = value;
