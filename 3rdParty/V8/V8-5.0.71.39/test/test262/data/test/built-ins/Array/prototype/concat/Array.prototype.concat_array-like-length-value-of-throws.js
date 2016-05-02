// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat array like length valueOf throws
---*/
function MyError() {}
var obj = {
  "length": {
    valueOf: function() {
      throw new MyError();
    },
    toString: null
  },
  "1": "A",
  "3": "B",
  "5": "C"
};
obj[Symbol.isConcatSpreadable] = true;
assert.throws(MyError, function() {
  [].concat(obj);
});
