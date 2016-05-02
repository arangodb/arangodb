// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-5-1
description: Array.prototype.map - thisArg not passed
flags: [noStrict]
includes: [fnGlobalObject.js]
---*/

    fnGlobalObject()._15_4_4_19_5_1 = true;

(function() {
    var _15_4_4_19_5_1 = false;
  
    function callbackfn(val, idx, obj) {
      return this._15_4_4_19_5_1;
    }
    var srcArr = [1];
    var resArr = srcArr.map(callbackfn);

assert.sameValue(resArr[0], true, 'resArr[0]');

})();
