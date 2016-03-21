// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat array like to length throws
---*/
var obj = {
  "length": {valueOf: null, toString: null},
  "1": "A",
  "3": "B",
  "5": "C"
};
obj[Symbol.isConcatSpreadable] = true;
var obj2 = { length: 3, "0": "0", "1": "1", "2": "2" };
var arr = ["X", "Y", "Z"];
assert.throws(TypeError, function() {
  Array.prototype.concat.call(obj, obj2, arr);
});
