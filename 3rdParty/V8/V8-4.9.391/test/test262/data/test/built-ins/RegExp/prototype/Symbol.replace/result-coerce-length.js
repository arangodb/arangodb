// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Type coercion of `length` property of result
es6id: 21.2.5.8
info: >
    [...]
    13. Repeat, while done is false
        a. Let result be RegExpExec(rx, S).
        [...]
    16. Repeat, for each result in results,
        a. Let nCaptures be ToLength(Get(result, "length")).
        [...]
features: [Symbol.replace]
---*/

var r = /./;
var counter = 0;
var coercibleIndex = {
  length: {
    valueOf: function() {
      return 3.9;
    }
  },
  0: '',
  1: 'foo',
  2: 'bar',
  3: 'baz'
};
r.exec = function() {
  return coercibleIndex;
};

assert.sameValue(r[Symbol.replace]('', '$1$2$3'), 'foobar$3');
