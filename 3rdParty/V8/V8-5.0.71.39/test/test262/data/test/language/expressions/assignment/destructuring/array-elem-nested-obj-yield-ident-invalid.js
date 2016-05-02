// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the Initializer of a nested
    destructuring assignment outside of a generator function body, it behaves
    as a IdentifierReference.
es6id: 12.14.5.3
flags: [onlyStrict]
negative: SyntaxError
---*/

[{ x = yield }] = [{}];
