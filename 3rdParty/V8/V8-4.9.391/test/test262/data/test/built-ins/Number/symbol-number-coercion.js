// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 7.1.3
description: >
    Number coercion operations on Symbols
---*/
assert.throws(TypeError, function() {
  Number(Symbol('66'));
});
assert.throws(TypeError, function() {
  Symbol('66') + 0;
});
assert.throws(TypeError, function() {
  +Symbol('66');
});
assert.throws(TypeError, function() {
  Symbol('66') >> 0;
});
assert.throws(TypeError, function() {
  Symbol('66') >>> 0;
});
assert.throws(TypeError, function() {
  Symbol('66') | 0;
});
assert.throws(TypeError, function() {
  new Uint16Array([Symbol('66')]);
});
