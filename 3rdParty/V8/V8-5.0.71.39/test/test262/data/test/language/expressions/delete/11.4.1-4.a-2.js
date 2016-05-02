// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    This test is actually testing the [[Delete]] internal method (8.12.8). Since the
    language provides no way to directly exercise [[Delete]], the tests are placed here.
es5id: 11.4.1-4.a-2
description: >
    delete operator returns true when deleting a configurable accessor
    property
---*/

  var o = {};

  // define an accessor
  // dummy getter
  var getter = function () { return 1; }
  var desc = { get: getter, configurable: true };
  Object.defineProperty(o, "foo", desc);
    
  var d = delete o.foo;

assert.sameValue(d, true, 'd');
assert.sameValue(o.hasOwnProperty("foo"), false, 'o.hasOwnProperty("foo")');
