// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.9
description: >
    get Set.prototype.size

    5. Let count be 0.
    6. For each e that is an element of entries
      a. If e is not empty, set count to count+1.

---*/

var s = new Set();

s.add(0);
s.add(undefined);
s.add(false);
s.add(NaN);
s.add(null);
s.add("");
s.add(Symbol());

assert.sameValue(s.size, 7, "The value of `s.size` is `7`");
