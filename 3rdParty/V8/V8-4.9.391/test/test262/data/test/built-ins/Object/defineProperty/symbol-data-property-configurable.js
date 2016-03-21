// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.1.2.4
description: >
    Symbol used as property for configurable data property definition
---*/
var sym = Symbol();
var obj = {};


Object.defineProperty(obj, sym, {
  value: 1,
  configurable: true
});

assert.sameValue(sym in obj, true, "The result of `sym in obj` is `true`");
assert.sameValue(
  Object.hasOwnProperty.call(obj, sym),
  true,
  "`Object.hasOwnProperty.call(obj, sym)` returns `true`"
);

var desc = Object.getOwnPropertyDescriptor(obj, sym);

assert.sameValue(desc.value, 1, "The value of `desc.value` is `1`");
assert.sameValue(desc.configurable, true, "The value of `desc.configurable` is `true`");
assert.sameValue(desc.writable, false, "The value of `desc.writable` is `false`");
assert.sameValue(desc.enumerable, false, "The value of `desc.enumerable` is `false`");
assert.sameValue(
  Object.prototype.propertyIsEnumerable.call(obj, sym),
  false,
  "`Object.prototype.propertyIsEnumerable.call(obj, sym)` returns `false`"
);

assert.sameValue(delete obj[sym], true, "The result of `delete obj[sym]` is `true`");

assert.sameValue(
  Object.getOwnPropertyDescriptor(obj, sym),
  undefined,
  "`Object.getOwnPropertyDescriptor(obj, sym)` returns `undefined`"
);
