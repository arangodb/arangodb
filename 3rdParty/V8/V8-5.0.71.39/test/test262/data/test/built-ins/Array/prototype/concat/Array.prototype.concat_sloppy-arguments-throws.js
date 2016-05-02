// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat sloppy arguments throws
---*/
function MyError() {}
var args = (function(a) { return arguments; })(1,2,3);
Object.defineProperty(args, 0, {
  get: function() { throw new MyError(); }
});
args[Symbol.isConcatSpreadable] = true;
assert.throws(MyError, function() {
  return [].concat(args, args);
});
