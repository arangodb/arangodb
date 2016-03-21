// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.4
description: >
    Set.prototype.delete ( value )

    ...
    6. Return false.

---*/

var s = new Set();

assert.sameValue(s.delete(1), false, "`s.delete(1)` returns `false`");
