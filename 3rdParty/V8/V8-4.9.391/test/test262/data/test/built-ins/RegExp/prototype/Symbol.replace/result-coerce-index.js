// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Type coercion of `index` property of result
es6id: 21.2.5.8
info: >
    [...]
    13. Repeat, while done is false
        a. Let result be RegExpExec(rx, S).
        [...]
    16. Repeat, for each result in results,
        [...]
        g. Let position be ToInteger(Get(result, "index")).
        [...]
features: [Symbol.replace]
---*/

var r = /./;
var counter = 0;
var coercibleIndex = {
  index: {
    valueOf: function() {
      return 2.9;
    }
  }
};
r.exec = function() {
  return coercibleIndex;
};

assert.sameValue(r[Symbol.replace]('abcd', ''), 'ab');
