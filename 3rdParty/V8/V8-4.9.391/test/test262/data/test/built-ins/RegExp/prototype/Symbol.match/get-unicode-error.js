// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Errors thrown by `unicode` accessor are forwarded to the runtime for global patterns
es6id: 21.2.5.6
info: >
    21.2.5.6 RegExp.prototype [ @@match ] ( string )

    [...]
    8. Else global is true,
       a. Let fullUnicode be ToBoolean(Get(rx, "unicode")).
       b. ReturnIfAbrupt(fullUnicode).
features: [Symbol.match]
---*/

var nonGlobalRe = /./;
var globalRe = /./g;
var accessor = function() {
  throw new Test262Error();
};
Object.defineProperty(nonGlobalRe, 'unicode', {
  get: accessor
});
Object.defineProperty(globalRe, 'unicode', {
  get: accessor
});

nonGlobalRe[Symbol.match]('');

assert.throws(Test262Error, function() {
  globalRe[Symbol.match]('');
});
