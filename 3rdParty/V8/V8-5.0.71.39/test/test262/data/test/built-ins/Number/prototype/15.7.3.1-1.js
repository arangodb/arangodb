// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.7.3.1-1
description: >
    Number.prototype is a data property with default attribute values
    (false)
---*/

  var d = Object.getOwnPropertyDescriptor(Number, 'prototype');
  

assert.sameValue(d.writable, false, 'd.writable');
assert.sameValue(d.enumerable, false, 'd.enumerable');
assert.sameValue(d.configurable, false, 'd.configurable');
