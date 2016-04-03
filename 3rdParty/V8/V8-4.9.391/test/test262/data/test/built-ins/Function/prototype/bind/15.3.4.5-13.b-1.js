// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5-13.b-1
description: Function.prototype.bind, bound fn has a 'length' own property
---*/

  function foo() { }
  var o = {};
  
  var bf = foo.bind(o);

assert(bf.hasOwnProperty('length'), 'bf.hasOwnProperty("length") !== true');
