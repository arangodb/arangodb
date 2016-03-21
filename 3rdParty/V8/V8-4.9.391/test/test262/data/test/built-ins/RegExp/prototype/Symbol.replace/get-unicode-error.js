// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Errors thrown by `unicode` accessor are forwarded to the runtime for global patterns
es6id: 21.2.5.8
info: >
    21.2.5.8 RegExp.prototype [ @@replace ] ( string, replaceValue )

    [...]
    10. If global is true, then
        a. Let fullUnicode be ToBoolean(Get(rx, "unicode")).
        b. ReturnIfAbrupt(fullUnicode).
features: [Symbol.replace]
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

nonGlobalRe[Symbol.replace]('', '');

assert.throws(Test262Error, function() {
  globalRe[Symbol.replace]('', '');
});
