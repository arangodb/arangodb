// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-5-1-s
description: Array.prototype.map - thisArg not passed to strict callbackfn
flags: [noStrict]
---*/

  var innerThisCorrect = false;
  
  function callbackfn(val, idx, obj) {
    "use strict";
    innerThisCorrect = this===undefined;
    return true;
  }

  [1].map(callbackfn);

assert(innerThisCorrect, 'innerThisCorrect !== true');
