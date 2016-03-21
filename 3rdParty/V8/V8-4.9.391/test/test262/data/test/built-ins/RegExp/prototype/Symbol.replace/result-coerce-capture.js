// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Type coercion of `1` property of result
es6id: 21.2.5.8
info: >
    [...]
    13. Repeat, while done is false
        a. Let result be RegExpExec(rx, S).
        [...]
    16. Repeat, for each result in results,
        [...]
        l. Repeat while n ≤ nCaptures
           i. Let capN be Get(result, ToString(n)).
           ii. ReturnIfAbrupt(capN).
           iii. If capN is not undefined, then
                1. Let capN be ToString(capN).
                [...]
features: [Symbol.replace]
---*/

var r = /./;
var coercibleValue = {
  length: 4,
  3: {
    toString: function() {
      return 'toString value';
    }
  }
};
r.exec = function() {
  return coercibleValue;
};

assert.sameValue(
  r[Symbol.replace]('', 'foo[$3]bar'), 'foo[toString value]bar'
);
