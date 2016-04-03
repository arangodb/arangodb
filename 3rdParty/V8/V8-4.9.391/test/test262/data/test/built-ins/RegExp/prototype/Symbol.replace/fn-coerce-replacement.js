// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: String coercion of the value returned by functional replaceValue
es6id: 21.2.5.8
info: >
    16. Repeat, for each result in results,
        [...]
        m. If functionalReplace is true, then
           i. Let replacerArgs be «matched».
           ii. Append in list order the elements of captures to the end of the
               List replacerArgs.
           iii. Append position and S as the last two elements of replacerArgs.
           iv. Let replValue be Call(replaceValue, undefined, replacerArgs).
           v. Let replacement be ToString(replValue).
        [...]
features: [Symbol.replace]
---*/

var replacer = function() {
  return {
    toString: function() {
      return 'toString value';
    }
  };
};

assert.sameValue(/x/[Symbol.replace]('[x]', replacer), '[toString value]');
