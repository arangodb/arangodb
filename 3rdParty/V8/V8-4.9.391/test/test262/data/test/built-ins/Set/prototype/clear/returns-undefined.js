// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.2
description: >
    Set.prototype.clear ( )

    ...
    6. Return undefined.

---*/

var s = new Set();

assert.sameValue(s.clear(), undefined, "`s.clear()` returns `undefined`");
