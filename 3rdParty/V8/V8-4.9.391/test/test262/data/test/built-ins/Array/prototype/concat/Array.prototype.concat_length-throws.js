// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat length throws
---*/
function MyError() {}
var obj = {};
obj[Symbol.isConcatSpreadable] = true;
Object.defineProperty(obj, "length", {
  get: function() { throw new MyError(); }
});

assert.throws(MyError, function() {
  [].concat(obj);
});

assert.throws(MyError, function() {
  Array.prototype.concat.call(obj, 1, 2, 3);
});
