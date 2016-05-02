// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5-15-1
description: Function.prototype.bind, 'length' is a data valued own property
---*/

  function foo() { }
  var o = {};
  
  var bf = foo.bind(o);
  var desc = Object.getOwnPropertyDescriptor(bf, 'length');

assert.sameValue(desc.hasOwnProperty('value'), true, 'desc.hasOwnProperty("value")');
assert.sameValue(desc.hasOwnProperty('get'), false, 'desc.hasOwnProperty("get")');
assert.sameValue(desc.hasOwnProperty('set'), false, 'desc.hasOwnProperty("set")');
