// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.3
description: >
  Calls the length setter if available
info: >
  Array.of ( ...items )

  ...
  9. Let setStatus be Set(A, "length", len, true).
  ...
---*/

var hits = 0;
var value;
var _this_;

function Pack() {
  Object.defineProperty(this, "length", {
    set: function(len) {
      hits++;
      value = len;
      _this_ = this;
    }
  });
}

var result = Array.of.call(Pack, 'wolves', 'cards', 'cigarettes', 'lies');

assert.sameValue(hits, 1, 'instance length setter called once');
assert.sameValue(
  value, 4,
  'setter is called with the number of Array.of arguments'
);
assert.sameValue(_this_, result, 'setter is called with the new object');
