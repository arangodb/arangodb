// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Type coercion of `0` property of result
es6id: 21.2.5.8
info: >
    [...]
    13. Repeat, while done is false
        a. Let result be RegExpExec(rx, S).
        [...]
    16. Repeat, for each result in results,
        [...]
        d. Let matched be ToString(Get(result, "0")).
        [...]
features: [Symbol.replace]
---*/

var r = /./;
var coercibleValue = {
  0: {
    toString: function() {
      return 'toString value';
    }
  }
};
r.exec = function() {
  return coercibleValue;
};

assert.sameValue(
  r[Symbol.replace]('', 'foo[$&]bar'), 'foo[toString value]bar'
);
